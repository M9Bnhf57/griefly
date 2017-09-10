#include "CoreInterface.h"

#include "AutogenMetadata.h"
#include "core/objects/MaterialObject.h"

namespace
{

kv::CoreInterface::ObjectsMetadata GenerateMetadata()
{
    kv::CoreInterface::ObjectsMetadata retval;

    for (auto it : (*GetItemsCreators()))
    {
        std::unique_ptr<kv::Object> object(it.second());
        kv::MaterialObject* material = CastTo<kv::MaterialObject>(object.get());
        if (!material)
        {
            qDebug() << QString("Type '%1' is not material object!").arg(it.first);
            continue;
        }

        kv::CoreInterface::ObjectMetadata metadata;
        metadata.name = it.first;
        metadata.default_view = *(material->GetView());

        retval.insert(metadata.name, metadata);
    }

    return retval;
}

}

namespace kv
{

class WorldImplementation : public WorldInterface
{
public:
    virtual void SaveWorld(QByteArray* data) const override
    {
        // TODO
        Q_UNUSED(data)
    }

    // TODO: Look into #360 properly
    virtual void ProcessNextTick(const QVector<Message>& messages) override
    {
        // TODO
        Q_UNUSED(messages)
    }

    virtual void Represent(GrowingFrame* frame) const override
    {
        // TODO
        Q_UNUSED(frame)
    }
    virtual quint32 Hash() const override
    {
        // TODO
        return 0;
    }
};

class CoreImplementation : public CoreInterface
{
public:
    virtual WorldPtr CreateWorldFromSave(const QByteArray& data) override
    {
        // TODO
        return nullptr;
    }
    virtual WorldPtr CreateWorldFromMapgen(const QByteArray& data)
    {
        // TODO
        return nullptr;
    }

    // <object name, metadata>
    virtual const ObjectsMetadata& GetObjectsMetadata() const override
    {
        static const ObjectsMetadata metadata = GenerateMetadata();
        return metadata;
    }
};

CoreInterface& GetCoreInstance()
{
    static CoreImplementation core;
    return core;
}

}
