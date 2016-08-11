#include "Atmos.h"

#include <QElapsedTimer>

#include <algorithm>

#include "../SyncRandom.h"
#include "../Helpers.h"

#include "AtmosGrid.h"

#include "representation/Text.h"

Atmosphere::Atmosphere(SyncRandom* random, IMapMaster* map, TextPainter *texts)
    : random_(random),
      map_(map),
      texts_(texts)
{
    grid_processing_ns_ = 0;
    load_grid_ns_ = 0;
    unload_grid_ns_ = 0;
    movement_processing_ns_ = 0;

    x_size_ = 0;
    y_size_ = 0;
    z_size_ = 0;

    (*texts_)["{Perf}AtmosGridProcessing"].SetUpdater
    ([&](std::string* str)
    {
        std::stringstream ss;
        ss << "Atmos grid processing: "
           << (grid_processing_ns_ * 1.0) / 1000000.0
           << " ms";
        *str = ss.str();
    }).SetFreq(1000);

    (*texts_)["{Perf}AtmosGridLoad"].SetUpdater
    ([&](std::string* str)
    {
        std::stringstream ss;
        ss << "Atmos grid load: "
           << (load_grid_ns_ * 1.0) / 1000000.0
           << " ms";
        *str = ss.str();
    }).SetFreq(1000);

    (*texts_)["{Perf}AtmosGridUnload"].SetUpdater
    ([&](std::string* str)
    {
        std::stringstream ss;
        ss << "Atmos grid unload: "
           << (unload_grid_ns_ * 1.0) / 1000000.0
           << " ms";
        *str = ss.str();
    }).SetFreq(1000);

    (*texts_)["{Perf}AtmosMove"].SetUpdater
    ([&](std::string* str)
    {
        std::stringstream ss;
        ss << "Atmos move processing: "
           << (movement_processing_ns_ * 1.0) / 1000000.0
           << " ms";
        *str = ss.str();
    }).SetFreq(1000);

    qDebug() << "Atmosphere load";
    z_size_ = 1;
    grid_ = nullptr;
}

void Atmosphere::Resize(size_t x, size_t y, size_t z)
{
    x_size_ = x;
    y_size_ = y;
    z_size_ = z;

    delete grid_;
    grid_ = new AtmosGrid(x_size_, y_size_);
}

void Atmosphere::Process()
{
    QElapsedTimer timer;
    timer.start();
    LoadDataToGrid();
    load_grid_ns_ = (load_grid_ns_ + timer.nsecsElapsed()) / 2;
    timer.start();
    grid_->Process();
    grid_processing_ns_ = (grid_processing_ns_ + timer.nsecsElapsed()) / 2;
    timer.start();
    UnloadDataFromGrid();
    unload_grid_ns_ = (unload_grid_ns_ + timer.nsecsElapsed()) / 2;
}

void Atmosphere::LoadDataToGrid()
{
    auto& squares = map_->GetSquares();
    for (int y = 0; y < map_->GetHeight(); ++y)
    {
        for (int x = 0; x < map_->GetWidth(); ++x)
        {
            auto tile = squares[y][x][0];

            AtmosGrid::Cell& cell = grid_->At(x, y);
            cell.ResetPassable();

            if (!CanPass(tile->GetPassable(D_UP), Passable::AIR))
            {
                cell.SetUnpassable(AtmosGrid::Cell::UP);
            }
            if (!CanPass(tile->GetPassable(D_DOWN), Passable::AIR))
            {
                cell.SetUnpassable(AtmosGrid::Cell::DOWN);
            }
            if (!CanPass(tile->GetPassable(D_LEFT), Passable::AIR))
            {
                cell.SetUnpassable(AtmosGrid::Cell::LEFT);
            }
            if (!CanPass(tile->GetPassable(D_RIGHT), Passable::AIR))
            {
                cell.SetUnpassable(AtmosGrid::Cell::RIGHT);
            }
            if (!CanPass(tile->GetPassable(D_ALL), Passable::AIR))
            {
                cell.SetUnpassable(AtmosGrid::Cell::CENTER);
            }

            if (tile->GetTurf()->GetAtmosState() == SPACE)
            {
                cell.flags |= AtmosGrid::Cell::SPACE;
            }

            AtmosHolder* holder = tile->GetAtmosHolder();

            cell.energy = holder->GetEnergy();

            for (int i = 0; i < GASES_NUM; ++i)
            {
                cell.gases[i] = holder->GetGase(i);
            }
        }
    }
}

void Atmosphere::UnloadDataFromGrid()
{
    auto& squares = map_->GetSquares();
    for (int y = 0; y < map_->GetHeight(); ++y)
    {
        for (int x = 0; x < map_->GetWidth(); ++x)
        {
            auto tile = squares[y][x][0];

            AtmosGrid::Cell& cell = grid_->At(x, y);


            AtmosHolder* holder = tile->GetAtmosHolder();
            holder->Truncate();
            if (cell.flags & AtmosGrid::Cell::SPACE)
            {
                continue;
            }

            for (int i = 0; i < GASES_NUM; ++i)
            {
                holder->AddGase(i, cell.gases[i]);
            }

            holder->AddEnergy(cell.energy);

            cell.pressure = holder->GetPressure();
        }
    }
}

const unsigned int PRESSURE_MOVE_BORDER = 1000;

void Atmosphere::ProcessTileMove(size_t x, size_t y, size_t z)
{   
    AtmosGrid::Cell& cell = grid_->At(x, y);

    if (cell.flags & AtmosGrid::Cell::SPACE)
    {
        return;
    }

    int max_diff = 0;
    Dir dir;

    auto tile = map_->GetSquares()[x][y][z];
    for (Dir local = 0; local < 4; ++local)
    {
        auto neighbour = tile->GetNeighbourImpl(local);
        int tile_pressure = tile->GetAtmosHolder()->GetPressure();
        int neighbour_pressure = neighbour->GetAtmosHolder()->GetPressure();
        int local_diff = tile_pressure - neighbour_pressure;
        if (local_diff > max_diff)
        {
            max_diff = local_diff;
            dir = local;
        }
    }

    if (max_diff < PRESSURE_MOVE_BORDER)
    {
        return;
    }

    IdPtr<CubeTile> neighbour = tile->GetNeighbourImpl(dir);

    if (!CanPass(tile->GetPassable(dir), Passable::AIR))
    {
        tile->BumpByGas(dir, true);
        return;
    }

    if (   !CanPass(neighbour->GetPassable(D_ALL), Passable::AIR)
        || !CanPass(neighbour->GetPassable(helpers::revert_dir(dir)), Passable::AIR))
    {
        neighbour->BumpByGas(dir);
        return;
    }

    if (tile->GetInsideList().size())
    {
        auto i = tile->GetInsideList().rbegin();
        while (   (i != tile->GetInsideList().rend())
               && ((*i)->passable_level == Passable::EMPTY))
        {
            ++i;
        }
        if (i != tile->GetInsideList().rend())
        {
            (*i)->ApplyForce(DirToVDir[dir]);
        }
    }
}

void Atmosphere::ProcessMove()
{
    QElapsedTimer timer;
    timer.start();
    // TODO: this takes abou 70% of atmos processing time
    // Some wind variable for each tile?
    for (int z = 0; z < z_size_; ++z)
    {
        for (int x = 0; x < x_size_; ++x)
        {
            for (int y = 0; y < y_size_; ++y)
            {
                ProcessTileMove(x, y, z);
            }
        }
    }
    movement_processing_ns_
        = (movement_processing_ns_ + timer.nsecsElapsed()) / 2;
}
