#include "simulation.h"
#include "mymath.h"
#include "threadingpool.h"

#include <stack>
#include <cstdio>

namespace IceHalo {

SimulationBufferData::SimulationBufferData()
    : pt{nullptr}, dir{nullptr}, w{nullptr}, faceId{nullptr}, raySeg{nullptr}, rayNum(0) {}


SimulationBufferData::~SimulationBufferData() {
  clean();
}


void SimulationBufferData::clean() {
  for (int i = 0; i < 2; i++) {
    deleteBuffer(i);
  }
}


void SimulationBufferData::deleteBuffer(int idx) {
  delete[] pt[idx];
  delete[] dir[idx];
  delete[] w[idx];
  delete[] faceId[idx];
  delete[] raySeg[idx];

  pt[idx] = nullptr;
  dir[idx] = nullptr;
  w[idx] = nullptr;
  faceId[idx] = nullptr;
  raySeg[idx] = nullptr;
}


void SimulationBufferData::allocate(size_t rayNum) {
  for (int i = 0; i < 2; i++) {
    auto tmpPt = new float[rayNum * 3];
    auto tmpDir = new float[rayNum * 3];
    auto tmpW = new float[rayNum];
    auto tmpFaceId = new int[rayNum];
    auto tmpRaySeg = new RaySegment*[rayNum];

    if (pt[i]) {
      size_t n = std::min(this->rayNum, rayNum);
      std::memcpy(tmpPt, pt[i], sizeof(float) * 3 * n);
      std::memcpy(tmpDir, dir[i], sizeof(float) * 3 * n);
      std::memcpy(tmpW, w[i], sizeof(float) * n);
      std::memcpy(tmpFaceId, faceId[i], sizeof(int) * n);
      std::memcpy(tmpRaySeg, raySeg[i], sizeof(void*) * n);

      deleteBuffer(i);
    }

    pt[i] = tmpPt;
    dir[i] = tmpDir;
    w[i] = tmpW;
    faceId[i] = tmpFaceId;
    raySeg[i] = tmpRaySeg;
  }
  this->rayNum = rayNum;
}


void SimulationBufferData::print() {
  std::printf("pt[0]                    dir[0]                   w[0]\n");
  for (decltype(rayNum) i = 0; i < rayNum; i++) {
    std::printf("%+.4f,%+.4f,%+.4f  ", pt[0][i * 3 + 0], pt[0][i * 3 + 1], pt[0][i * 3 + 2]);
    std::printf("%+.4f,%+.4f,%+.4f  ", dir[0][i * 3 + 0], dir[0][i * 3 + 1], dir[0][i * 3 + 2]);
    std::printf("%+.4f\n", w[0][i]);
  }

  std::printf("pt[1]                    dir[1]                   w[1]\n");
  for (decltype(rayNum) i = 0; i < rayNum; i++) {
    std::printf("%+.4f,%+.4f,%+.4f  ", pt[1][i * 3 + 0], pt[1][i * 3 + 1], pt[1][i * 3 + 2]);
    std::printf("%+.4f,%+.4f,%+.4f  ", dir[1][i * 3 + 0], dir[1][i * 3 + 1], dir[1][i * 3 + 2]);
    std::printf("%+.4f\n", w[1][i]);
  }
}


Simulator::Simulator(const SimulationContextPtr& context)
    : context(context), totalRayNum(0), activeRayNum(0), bufferSize(0) {}


// Start simulation
void Simulator::start() {
  context->fillActiveCrystal(&activeCrystalCtxs);
  totalRayNum = context->getTotalInitRays();
  auto msNum = context->getMultiScatterNum();

  rays.clear();
  final_ray_segments_.clear();
  RaySegmentPool::getInstance().clear();

  for (int i = 0; i < msNum; i++) {
    rays.emplace_back();

    for (const auto& ctx : activeCrystalCtxs) {
      activeRayNum = static_cast<size_t>(ctx->getPopulation() * totalRayNum);
      if (bufferSize < activeRayNum * kBufferSizeFactor) {
        bufferSize = activeRayNum * kBufferSizeFactor;
        buffer.allocate(bufferSize);
      }
      if (i == 0) {
        initSunRays();
      }
      initEntryRays(ctx, i);   // totalRayNum may be updated
      traceRays(ctx->getCrystal());
    }
    if (i < msNum - 1) {
      restoreResultRays(i);    // totalRayNum is updated.
    }
  }
}


// Init sun rays, and fill into dir[1]. They will be rotated and fill into dir[0] in initEntryRays().
// In world frame.
void Simulator::initSunRays() {
  float sunR = context->getSunDiameter() / 2;   // In degree
  const float* sunRayDir = context->getSunRayDir();
  auto& sampler = Math::RandomSampler::GetInstance();
  sampler.SampleSphericalPointsCart(sunRayDir, sunR, buffer.dir[1], activeRayNum);
}


// Init entry rays into a crystal. Fill pt[0], faceId[0], w[0] and raySeg[0].
// Rotate entry rays into crystal frame
// Add RayPtr and main axis rotation
void Simulator::initEntryRays(const CrystalContextPtr& ctx, int multiScatterIdx) {
  auto crystal = ctx->getCrystal();
  auto totalFaces = crystal->totalFaces();

  auto* faceArea = new float[totalFaces];
  auto* faceNorm = new float[totalFaces * 3];
  auto* facePoint = new float[totalFaces * 9];
  auto* prob = new float[totalFaces];

  crystal->copyFaceAreaData(faceArea);
  crystal->copyNormData(faceNorm);
  crystal->copyFaceData(facePoint);

  rays[multiScatterIdx].reserve(activeRayNum);

  auto& pool = RaySegmentPool::getInstance();
  auto& rng = Math::RandomNumberGenerator::GetInstance();
  auto& sampler = Math::RandomSampler::GetInstance();
  float axis_rot[3];
  for (decltype(activeRayNum) i = 0; i < activeRayNum; i++) {
    sampler.SampleSphericalPointsSph(ctx->getAxisDist(), ctx->getAxisMean(), ctx->getAxisStd(), axis_rot);
    axis_rot[2] = rng.get(ctx->getRollDist(), ctx->getRollMean(), ctx->getRollStd()) * Math::kDegreeToRad;
    Math::rotateZ(axis_rot, buffer.dir[1] + i * 3, buffer.dir[0] + i * 3);

    float sum = 0;
    for (int j = 0; j < totalFaces; j++) {
      prob[j] = std::max(-Math::dot3(faceNorm + j * 3, buffer.dir[0] + i * 3) * faceArea[j], 0.0f);
      sum += prob[j];
    }
    for (int j = 0; j < totalFaces; j++) {
      prob[j] /= sum;
    }

    buffer.faceId[0][i] = sampler.SampleInt(prob, totalFaces);
    sampler.SampleTriangularPoints(facePoint + buffer.faceId[0][i] * 9, buffer.pt[0] + i * 3);

    buffer.w[0][i] = 1.0f;

    auto r = pool.getRaySegment(buffer.pt[0] + i * 3, buffer.dir[0] + i * 3, buffer.w[0][i], buffer.faceId[0][i]);
    buffer.raySeg[0][i] = r;
    rays[multiScatterIdx].emplace_back(std::make_shared<Ray>(r, axis_rot));
    r->root = rays[multiScatterIdx].back().get();
  }

  delete[] faceArea;
  delete[] faceNorm;
  delete[] facePoint;
  delete[] prob;
}


// Restore and shuffle resulted rays, and fill into dir[0].
void Simulator::restoreResultRays(int multiScatterIdx) {
  final_ray_segments_.clear();

  auto& rng = Math::RandomNumberGenerator::GetInstance();
  std::stack<RaySegment*> s;
  size_t idx = 0;
  for (auto& r : rays[multiScatterIdx]) {
    s.push(r->firstRaySeg);
    while (!s.empty()) {
      auto tmp_r = s.top();
      s.pop();

      if (tmp_r->isFinished && tmp_r->w > SimulationContext::kScatMinW &&
          rng.getUniform() < context->getMultiScatterProb()) {
        assert(tmp_r->root);
        const auto axis_rot = tmp_r->root->main_axis_rot.val();
        Math::rotateZBack(axis_rot, tmp_r->dir.val(), buffer.dir[0] + idx * 3);
        idx++;
      } else {
        if (tmp_r->nextReflect) {
          s.push(tmp_r->nextReflect);
        }
        if (tmp_r->nextRefract) {
          s.push(tmp_r->nextRefract);
        }
      }
    }
  }
  totalRayNum = idx;

  // Shuffle
  auto& sampler = Math::RandomSampler::GetInstance();
  float tmp_dir[3];
  for (decltype(totalRayNum) i = 0; i < totalRayNum; i++) {
    int tmp_idx = sampler.SampleInt(static_cast<int>(totalRayNum - i));
    std::memcpy(tmp_dir, buffer.dir[0] + (i + tmp_idx) * 3, sizeof(float) * 3);
    std::memcpy(buffer.dir[0] + i * 3, tmp_dir, sizeof(float) * 3);
    std::memcpy(buffer.dir[0] + (i + tmp_idx) * 3, buffer.dir[0] + i * 3, sizeof(float) * 3);
  }
}


// Trace rays.
// Start from dir[0] and pt[0].
void Simulator::traceRays(const CrystalPtr& crystal) {
  auto pool = Pool::getInstance();

  int maxRecursionNum = context->getMaxRecursionNum();
  float n = IceRefractiveIndex::n(context->getCurrentWavelength());
  for (int i = 0; i < maxRecursionNum; i++) {
    if (bufferSize < activeRayNum * 2) {
      bufferSize = activeRayNum * kBufferSizeFactor;
      buffer.allocate(bufferSize);
    }
    auto step = std::max(activeRayNum / 100, static_cast<size_t>(10));
    for (decltype(activeRayNum) j = 0; j < activeRayNum; j += step) {
      decltype(activeRayNum) current_num = std::min(activeRayNum - j, step);
      pool->addJob([=]{
        Optics::HitSurface(crystal, n, current_num,
          buffer.dir[0] + j * 3, buffer.faceId[0] + j, buffer.w[0] + j,
          buffer.dir[1] + j * 6, buffer.w[1] + j * 2);
        Optics::Propagate(crystal, current_num * 2,
          buffer.pt[0] + j * 3, buffer.dir[1] + j * 6, buffer.w[1] + j * 2,
          buffer.pt[1] + j * 6, buffer.faceId[1] + j * 2);
      });
    }
    pool->waitFinish();
    saveRaySegments();
    refreshBuffer();    // activeRayNum is updated.
  }
}


// Save rays
void Simulator::saveRaySegments() {
  for (size_t i = 0; i < activeRayNum * 2; i++) {
    buffer.raySeg[1][i] = nullptr;
  }

  auto& pool = RaySegmentPool::getInstance();
  for (size_t i = 0; i < activeRayNum * 2; i++) {
    if (buffer.w[1][i] <= 0) {   // For refractive rays in total reflection case
      continue;
    }

    auto r = pool.getRaySegment(buffer.pt[0] + i / 2 * 3, buffer.dir[1] + i * 3, buffer.w[1][i], buffer.faceId[0][i / 2]);
    if (buffer.faceId[1][i] < 0) {
      r->isFinished = true;
    }
    if (r->isFinished || r->w < SimulationContext::kPropMinW) {
      final_ray_segments_.emplace_back(r);
    }

    auto prevRaySeg = buffer.raySeg[0][i / 2];
    if (i % 2 == 0) {
      prevRaySeg->nextReflect = r;
    } else {
      prevRaySeg->nextRefract = r;
    }
    r->prev = prevRaySeg;
    r->root = prevRaySeg->root;
    buffer.raySeg[1][i] = r;
  }
}


// Squeeze data, copy into another buffer (from buf[1] to buf[0])
// Update activeRayNum.
void Simulator::refreshBuffer() {
  size_t idx = 0;
  for (size_t i = 0; i < activeRayNum * 2; i++) {
    if (buffer.raySeg[1][i] && buffer.faceId[1][i] >= 0 && buffer.w[1][i] > SimulationContext::kPropMinW) {
      std::memcpy(buffer.pt[0] + idx * 3, buffer.pt[1] + i * 3, sizeof(float) * 3);
      std::memcpy(buffer.dir[0] + idx * 3, buffer.dir[1] + i * 3, sizeof(float) * 3);
      buffer.w[0][idx] = buffer.w[1][i];
      buffer.faceId[0][idx] = buffer.faceId[1][i];
      buffer.raySeg[0][idx] = buffer.raySeg[1][i];
      idx++;
    }
  }
  activeRayNum = idx;
}


void Simulator::saveFinalDirections(const char* filename) {
  File file(context->getDataDirectory().c_str(), filename);
  if (!file.open(OpenMode::kWrite | OpenMode::kBinary)) return;

  file.write(context->getCurrentWavelength());

  float finalDir[3];
  for (const auto& r : final_ray_segments_) {
    assert(r->root);
    const auto axis_rot = r->root->main_axis_rot.val();
    Math::rotateZBack(axis_rot, r->dir.val(), finalDir);
    file.write(finalDir, 3);
    file.write(r->w);
  }

  file.close();
}


void Simulator::printRayInfo() {
  std::stack<RaySegment*> s;
  for (const auto& rs : rays) {
    for (const auto& r : rs) {
      s.push(r->firstRaySeg);

      while (!s.empty()) {
        auto p = s.top();
        s.pop();
        if (p->nextRefract && !p->isFinished) {
          s.push(p->nextRefract);
        }
        if (p->nextReflect && !p->isFinished) {
          s.push(p->nextReflect);
        }
        if (!p->nextReflect && !p->nextRefract && p->isValidEnd()) {
          std::stack<RaySegment*> tmp_stack;
          tmp_stack.push(p);
          while (p->prev) {
            tmp_stack.push(p->prev);
            p = p->prev;
          }

          std::printf("%li,0,0,0,0,0,-1\n", tmp_stack.size());
          while (!tmp_stack.empty()) {
            p = tmp_stack.top();
            tmp_stack.pop();
            std::printf("%+.4f,%+.4f,%+.4f,%+.4f,%+.4f,%+.4f,%+.4f\n",
              p->pt.x(), p->pt.y(), p->pt.z(),
              p->dir.x(), p->dir.y(), p->dir.z(),
              p->w);
          }
        }
      }
    }
  }
}

}  // namespace IceHalo