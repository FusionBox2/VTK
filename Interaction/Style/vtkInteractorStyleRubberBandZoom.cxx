// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause
#include "vtkInteractorStyleRubberBandZoom.h"

#include "vtkCamera.h"
#include "vtkMath.h"
#include "vtkObjectFactory.h"
#include "vtkRenderWindow.h"
#include "vtkRenderWindowInteractor.h"
#include "vtkRenderer.h"
#include "vtkUnsignedCharArray.h"
#include "vtkVector.h"

VTK_ABI_NAMESPACE_BEGIN
vtkStandardNewMacro(vtkInteractorStyleRubberBandZoom);

vtkInteractorStyleRubberBandZoom::vtkInteractorStyleRubberBandZoom()
{
  this->StartPosition[0] = this->StartPosition[1] = 0;
  this->EndPosition[0] = this->EndPosition[1] = 0;
  this->Moving = 0;
  this->LockAspectToViewport = false;
  this->CenterAtStartPosition = false;
  this->UseDollyForPerspectiveProjection = true;
  this->PixelArray = vtkUnsignedCharArray::New();
}

vtkInteractorStyleRubberBandZoom::~vtkInteractorStyleRubberBandZoom()
{
  this->PixelArray->Delete();
}

void vtkInteractorStyleRubberBandZoom::AdjustBox(int startPosition[2], int endPosition[2]) const
{
  if (this->LockAspectToViewport && this->CurrentRenderer != nullptr)
  {
    double aspect = this->CurrentRenderer->GetAspect()[0];

    int dx = endPosition[0] - startPosition[0];
    int dy = endPosition[1] - startPosition[1];

    // now we could adjust dx or dy. We pick one that ensures that the current end
    // position is always included in the new box. That way, the mouse is not
    // floating outside of the box.
    const int newDY = static_cast<int>(0.5 + std::abs(dx) / aspect);
    if (std::abs(dy) > newDY)
    {
      const int newDX = static_cast<int>(0.5 + aspect * std::abs(dy));
      dx = (dx < 0) ? -newDX : newDX;
    }
    else
    {
      dy = (dy < 0) ? -newDY : newDY;
    }
    endPosition[0] = startPosition[0] + dx;
    endPosition[1] = startPosition[1] + dy;
  }

  bool centerAtStartPosition = this->CenterAtStartPosition;
  if (this->Interactor && (this->Interactor->GetControlKey() || this->Interactor->GetShiftKey()))
  {
    centerAtStartPosition = !centerAtStartPosition;
  }
  if (centerAtStartPosition)
  {
    vtkVector2i dia;
    dia = vtkVector2i(endPosition) - vtkVector2i(startPosition);
    vtkVector2i newStartPosition = vtkVector2i(startPosition) - dia;
    startPosition[0] = newStartPosition.GetX();
    startPosition[1] = newStartPosition.GetY();
  }
}

void vtkInteractorStyleRubberBandZoom::OnMouseMove()
{
  if (!this->Interactor || !this->Moving)
  {
    return;
  }

  this->EndPosition[0] = this->Interactor->GetEventPosition()[0];
  this->EndPosition[1] = this->Interactor->GetEventPosition()[1];
  const int* size = this->Interactor->GetRenderWindow()->GetSize();
  if (this->EndPosition[0] > (size[0] - 1))
  {
    this->EndPosition[0] = size[0] - 1;
  }
  if (this->EndPosition[0] < 0)
  {
    this->EndPosition[0] = 0;
  }
  if (this->EndPosition[1] > (size[1] - 1))
  {
    this->EndPosition[1] = size[1] - 1;
  }
  if (this->EndPosition[1] < 0)
  {
    this->EndPosition[1] = 0;
  }

  int startPosition[2] = { this->StartPosition[0], this->StartPosition[1] };
  int endPosition[2] = { this->EndPosition[0], this->EndPosition[1] };
  // Adjust box to fit aspect ratio, if needed.
  this->AdjustBox(startPosition, endPosition);

  vtkUnsignedCharArray* tmpPixelArray = vtkUnsignedCharArray::New();
  tmpPixelArray->DeepCopy(this->PixelArray);

  unsigned char* pixels = tmpPixelArray->GetPointer(0);

  int minX = vtkMath::Min(startPosition[0], endPosition[0]);
  int minY = vtkMath::Min(startPosition[1], endPosition[1]);
  int maxX = vtkMath::Max(startPosition[0], endPosition[0]);
  int maxY = vtkMath::Max(startPosition[1], endPosition[1]);

  int clampedMinX = vtkMath::Max(minX, 0);
  int clampedMaxX = vtkMath::Min(maxX, size[0] - 1);
  int clampedMinY = vtkMath::Max(minY, 0);
  int clampedMaxY = vtkMath::Min(maxY, size[1] - 1);

  // Draw zoom box
  // Draw bottom horizontal line
  if (minY >= 0 && minY < size[1])
  {
    for (int i = clampedMinX; i < clampedMaxX; ++i)
    {
      pixels[3 * (minY * size[0] + i)] = 255 ^ pixels[3 * (minY * size[0] + i)];
      pixels[3 * (minY * size[0] + i) + 1] = 255 ^ pixels[3 * (minY * size[0] + i) + 1];
      pixels[3 * (minY * size[0] + i) + 2] = 255 ^ pixels[3 * (minY * size[0] + i) + 2];
    }
  }

  // Draw top horizontal line
  if (maxY >= 0 && maxY < size[1])
  {
    for (int i = clampedMinX; i < clampedMaxX; ++i)
    {
      pixels[3 * (maxY * size[0] + i)] = 255 ^ pixels[3 * (maxY * size[0] + i)];
      pixels[3 * (maxY * size[0] + i) + 1] = 255 ^ pixels[3 * (maxY * size[0] + i) + 1];
      pixels[3 * (maxY * size[0] + i) + 2] = 255 ^ pixels[3 * (maxY * size[0] + i) + 2];
    }
  }

  // Draw left vertical line
  if (minX >= 0 && minX < size[0])
  {
    for (int i = clampedMinY; i < clampedMaxY; ++i)
    {
      pixels[3 * (i * size[0] + minX)] = 255 ^ pixels[3 * (i * size[0] + minX)];
      pixels[3 * (i * size[0] + minX) + 1] = 255 ^ pixels[3 * (i * size[0] + minX) + 1];
      pixels[3 * (i * size[0] + minX) + 2] = 255 ^ pixels[3 * (i * size[0] + minX) + 2];
    }
  }

  // Draw right vertical line
  if (maxX >= 0 && maxX < size[0])
  {
    for (int i = clampedMinY; i < clampedMaxY; ++i)
    {
      pixels[3 * (i * size[0] + maxX)] = 255 ^ pixels[3 * (i * size[0] + maxX)];
      pixels[3 * (i * size[0] + maxX) + 1] = 255 ^ pixels[3 * (i * size[0] + maxX) + 1];
      pixels[3 * (i * size[0] + maxX) + 2] = 255 ^ pixels[3 * (i * size[0] + maxX) + 2];
    }
  }

  this->Interactor->GetRenderWindow()->SetPixelData(0, 0, size[0] - 1, size[1] - 1, pixels, 0);
  this->Interactor->GetRenderWindow()->Frame();

  tmpPixelArray->Delete();
}

void vtkInteractorStyleRubberBandZoom::OnLeftButtonDown()
{
  if (!this->Interactor)
  {
    return;
  }
  this->Moving = 1;

  vtkRenderWindow* renWin = this->Interactor->GetRenderWindow();

  this->StartPosition[0] = this->Interactor->GetEventPosition()[0];
  this->StartPosition[1] = this->Interactor->GetEventPosition()[1];
  this->EndPosition[0] = this->StartPosition[0];
  this->EndPosition[1] = this->StartPosition[1];

  this->PixelArray->Initialize();
  this->PixelArray->SetNumberOfComponents(3);
  const int* size = renWin->GetSize();
  this->PixelArray->SetNumberOfTuples(size[0] * size[1]);

  renWin->GetPixelData(0, 0, size[0] - 1, size[1] - 1, 1, this->PixelArray);

  this->FindPokedRenderer(this->StartPosition[0], this->StartPosition[1]);
  if (this->CurrentRenderer)
  {
    // Ensure the aspect ratio is up-to-date.
    this->CurrentRenderer->ComputeAspect();
  }
}

void vtkInteractorStyleRubberBandZoom::OnLeftButtonUp()
{
  if (!this->Interactor || !this->Moving)
  {
    return;
  }

  if ((this->StartPosition[0] != this->EndPosition[0]) ||
    (this->StartPosition[1] != this->EndPosition[1]))
  {
    this->Zoom();
  }
  this->Moving = 0;
}

void vtkInteractorStyleRubberBandZoom::Zoom()
{
  int startPosition[2] = { this->StartPosition[0], this->StartPosition[1] };
  int endPosition[2] = { this->EndPosition[0], this->EndPosition[1] };

  // Adjust box to fit aspect ratio, if needed.
  this->AdjustBox(startPosition, endPosition);

  const vtkRecti box(startPosition[0] < endPosition[0] ? startPosition[0] : endPosition[0],
    startPosition[1] < endPosition[1] ? startPosition[1] : endPosition[1],
    std::abs(endPosition[0] - startPosition[0]), std::abs(endPosition[1] - startPosition[1]));

  vtkCamera* cam = this->CurrentRenderer->GetActiveCamera();
  if (cam->GetParallelProjection() || this->UseDollyForPerspectiveProjection)
  {
    this->ZoomTraditional(box);
  }
  else
  {
    cam->SetFocalPoint(this->CalculatePerspectiveZoomFocalPoint(box).GetData());
    this->CurrentRenderer->ZoomToBoxUsingViewAngle(box);
  }
  this->Interactor->Render();
}

vtkVector3d vtkInteractorStyleRubberBandZoom::CalculatePerspectiveZoomFocalPoint(
  const vtkRecti& box) const
{
  const vtkVector2d rbCenter2d = box.GetCenter();
  const vtkVector3d rbCenter3d = vtkVector3d(rbCenter2d.GetX(), rbCenter2d.GetY(), 0.0);
  const vtkVector3d worldRBCenter = this->CurrentRenderer->DisplayToWorld(rbCenter3d);

  return worldRBCenter;
}

void vtkInteractorStyleRubberBandZoom::ZoomTraditional(const vtkRecti& box)
{
  const int* size = this->CurrentRenderer->GetSize();
  const int* origin = this->CurrentRenderer->GetOrigin();
  vtkCamera* cam = this->CurrentRenderer->GetActiveCamera();

  const vtkVector2d rbCenter2d = box.GetCenter();
  const vtkVector3d rbCenter3d = vtkVector3d(rbCenter2d.GetX(), rbCenter2d.GetY(), 0.0);
  const vtkVector3d worldRBCenter = this->CurrentRenderer->DisplayToWorld(rbCenter3d);

  const vtkVector2d winCenter2d = vtkRecti(origin[0], origin[1], size[0], size[1]).GetCenter();
  const vtkVector3d winCenter3d = vtkVector3d(winCenter2d.GetX(), winCenter2d.GetY(), 0.0);
  const vtkVector3d worldWinCenter = this->CurrentRenderer->DisplayToWorld(winCenter3d);
  const vtkVector3d translation = worldRBCenter - worldWinCenter;

  vtkVector3d pos, fp;
  cam->GetPosition(pos.GetData());
  cam->GetFocalPoint(fp.GetData());

  pos = pos + translation;
  fp = fp + translation;

  cam->SetPosition(pos.GetData());
  cam->SetFocalPoint(fp.GetData());

  double zoomFactor;
  if (box.GetWidth() > box.GetHeight())
  {
    zoomFactor = size[0] / static_cast<double>(box.GetWidth());
  }
  else
  {
    zoomFactor = size[1] / static_cast<double>(box.GetHeight());
  }

  if (cam->GetParallelProjection())
  {
    cam->Zoom(zoomFactor);
  }
  else
  {
    // In perspective mode, zoom in by moving the camera closer.  Because we are
    // moving the camera closer, we have to be careful to try to adjust the
    // clipping planes to best match the actual position they were in before.
    double initialDistance = cam->GetDistance();
    cam->Dolly(zoomFactor);

    double finalDistance = cam->GetDistance();
    double deltaDistance = initialDistance - finalDistance;
    double clippingRange[2];
    cam->GetClippingRange(clippingRange);
    clippingRange[0] -= deltaDistance;
    clippingRange[1] -= deltaDistance;
    // Correct bringing clipping planes too close or behind camera.
    if (clippingRange[1] <= 0.0)
    {
      clippingRange[1] = 0.001;
    }
    // This near plane check comes from vtkRenderer::ResetCameraClippingRange()
    if (clippingRange[0] < 0.001 * clippingRange[1])
    {
      clippingRange[0] = 0.001 * clippingRange[1];
    }
    cam->SetClippingRange(clippingRange);
  }
}

void vtkInteractorStyleRubberBandZoom::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
  os << indent << "LockAspectToViewport: " << this->LockAspectToViewport << endl;
  os << indent << "CenterAtStartPosition: " << this->CenterAtStartPosition << endl;
  os << indent << "UseDollyForPerspectiveProjection: " << this->UseDollyForPerspectiveProjection
     << endl;
}
VTK_ABI_NAMESPACE_END
