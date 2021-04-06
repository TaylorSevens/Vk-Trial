#pragma once
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/trigonometric.hpp>
#include <glm/gtc/matrix_transform.hpp>

class ArcBallCamera {
public:
  ArcBallCamera();
  ArcBallCamera(glm::vec3 target, float radius, float pitch, float yaw);

  void SetLens(float fovY, float aspect, float znear, float zfar);
  void SetOrtho(float xmin, float xmax, float ymin, float ymax, float zmin, float zmax);
  void SetPositions(glm::vec3 target, glm::vec3 eye);
  void SetOrbit(glm::vec3 target, float r, float pitch, float yaw);
  void Rotate(float dx, float dy, float scale = 1.0f);
  void Zoom(float dr, float rmin, float rmax, float scale = 1.0f);

  glm::mat4 GetView() const;
  glm::mat4 GetViewProj() const;
  glm::mat4 GetProj() const;

private:
  void UpdateViewMatrix();

  float radius_;
  float pitch_; // rotate about x axis
  float yaw_;   // rotate about y axis
  glm::vec3 target_;
  glm::vec3 eye_;
  glm::mat4 view_;
  glm::mat4 proj_;
  glm::mat4 viewproj_cached_;
};

ArcBallCamera::ArcBallCamera() {
  target_ = glm::vec3(0.0f);
  radius_ = 1.0f;
  pitch_ = .0f;
  yaw_ = .0f;
  proj_ = glm::perspectiveLH(glm::radians(45.f), 1.f, 1.f, 1000.f);
  UpdateViewMatrix();
}

ArcBallCamera::ArcBallCamera(glm::vec3 target, float radius, float pitch, float yaw) {
  target_ = target;
  radius_ = radius;
  pitch_ = pitch;
  if (pitch_ < .0f)
    pitch_ = .0f;
  else if (pitch_ > glm::pi<float>() - 0.1f)
    pitch_ = glm::pi<float>() - 0.1f;
  yaw_ = yaw;
  proj_ = glm::perspectiveLH(glm::radians(45.f), 1.f, 1.f, 1000.f);
  UpdateViewMatrix();
}

inline void ArcBallCamera::SetLens(float fovY, float aspect, float znear, float zfar) {
  proj_ = glm::perspectiveLH(fovY, aspect, znear, zfar);
  viewproj_cached_ = proj_ * view_;
}

inline void ArcBallCamera::SetOrtho(float xmin, float xmax, float ymin, float ymax, float zmin,
                                    float zmax) {
  proj_ = glm::orthoLH(xmin, xmax, ymin, ymax, zmin, zmax);
  viewproj_cached_ = proj_ * view_;
}

inline void ArcBallCamera::SetPositions(glm::vec3 target, glm::vec3 eye) {
  glm::vec3 v = eye - target;
  radius_ = glm::length(v);
  v /= radius_;
  yaw_ = atan2(v.z, v.x);
  pitch_ = acos(v.y);
  UpdateViewMatrix();
}

inline void ArcBallCamera::SetOrbit(glm::vec3 target, float r, float pitch, float yaw) {
  radius_ = r;
  yaw_ = yaw;
  pitch_ = pitch;
  if (pitch_ < .0f)
    pitch_ = .0f;
  else if (pitch_ > glm::pi<float>() - 0.1f)
    pitch_ = glm::pi<float>() - 0.1f;
  UpdateViewMatrix();
}

inline void ArcBallCamera::Rotate(float dx, float dy, float scale) {
  pitch_ += dy * scale;
  yaw_ += dx * scale;

  if (pitch_ < .0f)
    pitch_ = .0f;
  else if (pitch_ > glm::pi<float>() - 0.1f)
    pitch_ = glm::pi<float>() - 0.1f;

  UpdateViewMatrix();
}

inline void ArcBallCamera::Zoom(float dr, float rmin, float rmax, float scale) {
  radius_ += dr * scale;

  if (radius_ < rmin)
    radius_ = rmin;
  else if (radius_ > rmax)
    radius_ = rmax;
  UpdateViewMatrix();
}

inline glm::mat4 ArcBallCamera::GetView() const { return view_; }

inline glm::mat4 ArcBallCamera::GetViewProj() const { return viewproj_cached_; }

inline glm::mat4 ArcBallCamera::GetProj() const { return proj_; }

void ArcBallCamera::UpdateViewMatrix() {
  glm::mat4 worldTrans = glm::translate(glm::mat4(1.0f), -target_);
  float sin_phi = sin(pitch_);

  glm::vec3 v{  radius_ * sin(yaw_) * sin_phi,  radius_ * cos(pitch_),  radius_ * cos(yaw_) * sin_phi};

  glm::mat4 view = glm::lookAtLH(v, glm::vec3(.0f), glm::vec3(.0f, 1.0f, .0f));

  view_ = view * worldTrans;
  viewproj_cached_ = proj_ * view_;
}
