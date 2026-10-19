// SPDX-License-Identifier: MIT
#include "CnaEngine/Extended/WorldCameraSync.hpp"

namespace CnaEngine::Extended
{
    bool SyncToWorldCamera(const Graphics::Camera& camera, CNA::Extended::World3DEXT::Camera3DEXT& target)
    {
        target.setPositionProperty(camera.Position());
        target.setTargetProperty(camera.Position() + camera.Forward());
        target.setUpProperty(camera.Up());
        target.setAspectRatioProperty(camera.AspectRatio());
        target.setNearPlaneProperty(camera.NearPlane());
        target.setFarPlaneProperty(camera.FarPlane());
        if (camera.Kind() != Graphics::ProjectionKind::Perspective) {
            return false;
        }
        target.setFieldOfViewProperty(camera.FieldOfView());
        return true;
    }

    void SyncFromWorldCamera(const CNA::Extended::World3DEXT::Camera3DEXT& source, Graphics::Camera& camera)
    {
        camera.SetPosition(source.getPositionProperty());
        camera.LookAt(source.getTargetProperty(), source.getUpProperty());
        camera.SetPerspective(source.getFieldOfViewProperty(), source.getAspectRatioProperty(),
                              source.getNearPlaneProperty(), source.getFarPlaneProperty());
    }
}
