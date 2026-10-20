// SPDX-License-Identifier: MIT
//
// Performance checks without budgets (REL-003). Absolute budgets need the
// owner and a reference machine; what can be checked anywhere is that the
// engine's per-frame CPU work grows as its algorithms promise. Each check
// times the same work at n and 16n items (best of several runs, so a noisy
// machine slows both) and fails if the ratio shows quadratic growth. The
// timings are printed and recorded as test properties.
#include "CnaEngine/Graphics/Camera.hpp"
#include "CnaEngine/Graphics/GpuMesh.hpp"
#include "CnaEngine/Graphics/Instancing.hpp"
#include "CnaEngine/Graphics/RenderQueue.hpp"
#include "CnaEngine/Graphics/Visibility.hpp"
#include "CnaEngine/Input/ActionMap.hpp"
#include "CnaEngine/Input/InputSnapshot.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace CnaEngine
{
    using Microsoft::Xna::Framework::BoundingSphere;
    using Microsoft::Xna::Framework::Matrix;
    using Microsoft::Xna::Framework::Vector3;

    namespace
    {
        constexpr int Small = 1000;
        constexpr int Large = 16 * Small;
        // 16x the items: linear work grows 16x, n log n about 22x, quadratic
        // 256x. The bound leaves room for cache effects and noise.
        constexpr double MaxRatio = 64.0;

        double BestSeconds(const std::function<void()>& work, const int runs = 5)
        {
            double best = 1e30;
            for (int i = 0; i < runs; ++i) {
                const auto start = std::chrono::steady_clock::now();
                work();
                best = std::min(best, std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count());
            }
            return best;
        }

        void Report(const char* name, const double small, const double large)
        {
            const double ratio = large / std::max(small, 1e-9);
            std::printf("%s: %d items %.3f ms, %d items %.3f ms, ratio %.1f\n", name, Small, small * 1e3, Large,
                        large * 1e3, ratio);
            ::testing::Test::RecordProperty(std::string(name) + "_small_us", static_cast<int>(small * 1e6));
            ::testing::Test::RecordProperty(std::string(name) + "_large_us", static_cast<int>(large * 1e6));
            EXPECT_LT(ratio, MaxRatio) << name << " grows faster than n log n";
        }

        Graphics::Camera ViewCamera()
        {
            Graphics::Camera camera;
            camera.SetPerspective(1.0f, 16.0f / 9.0f, 0.1f, 500.0f);
            camera.SetPosition(Vector3(0.0f, 2.0f, 0.0f));
            return camera;
        }

        // No GPU work is timed: items only need a mesh to point at.
        const Graphics::GpuMesh& Mesh()
        {
            static const Graphics::GpuMesh mesh;
            return mesh;
        }

        // Items scattered over a 400 m square around the camera, 8 materials.
        std::vector<Graphics::RenderItem> Items(const int count, const std::vector<Graphics::Material>& materials)
        {
            std::vector<Graphics::RenderItem> items(static_cast<std::size_t>(count));
            unsigned state = 7u;
            for (auto& item : items) {
                state = state * 1664525u + 1013904223u;
                const float x = static_cast<float>(state >> 8 & 0xFFFF) / 65535.0f * 400.0f - 200.0f;
                state = state * 1664525u + 1013904223u;
                const float z = static_cast<float>(state >> 8 & 0xFFFF) / 65535.0f * 400.0f - 200.0f;
                item.mesh = &Mesh();
                item.material = &materials[(state >> 3) % materials.size()];
                item.world = Matrix::CreateTranslation(x, 0.0f, z);
                item.bounds = BoundingSphere(Vector3(x, 0.0f, z), 1.0f);
            }
            return items;
        }
    }

    TEST(PerformanceScaling, CullAndSortGrowAsNLogN)
    {
        const std::vector<Graphics::Material> materials(8);
        const auto small = Items(Small, materials);
        const auto large = Items(Large, materials);
        const auto camera = ViewCamera();
        Graphics::VisibilitySettings settings;
        settings.maxDistance = 300.0f;
        auto frame = [&](const std::vector<Graphics::RenderItem>& items) {
            Graphics::RenderQueue queue;
            Graphics::CullStatistics stats;
            Graphics::CullInto(items, camera, settings, queue, stats);
            queue.Sort(camera.Position());
        };
        Report("cull_and_sort", BestSeconds([&] { frame(small); }), BestSeconds([&] { frame(large); }));
    }

    TEST(PerformanceScaling, InstanceCullingIsLinear)
    {
        auto makeSet = [](const int count) {
            Graphics::InstanceSet set(&Mesh());
            for (int i = 0; i < count; ++i) {
                set.Add(Matrix::CreateTranslation(static_cast<float>(i % 200) - 100.0f, 0.0f,
                                                  -static_cast<float>(i / 200)));
            }
            return set;
        };
        auto small = makeSet(Small);
        auto large = makeSet(Large);
        const auto camera = ViewCamera();
        Graphics::VisibilitySettings settings;
        Graphics::CullStatistics stats;
        Report("instance_cull", BestSeconds([&] { small.Cull(camera, settings, stats); }),
               BestSeconds([&] { large.Cull(camera, settings, stats); }));
    }

    TEST(PerformanceScaling, ActionUpdatesAreLinearInActions)
    {
        using Microsoft::Xna::Framework::Input::Keys;
        auto makeMap = [](const int count) {
            Input::ActionMap actions;
            for (int i = 0; i < count; ++i) {
                const auto action = actions.AddAction("Action" + std::to_string(i));
                actions.Bind(action, Input::Binding::ForKey(static_cast<Keys>(static_cast<int>(Keys::A) + i % 26)));
            }
            return actions;
        };
        auto small = makeMap(Small / 10);
        auto large = makeMap(Large / 10);
        Input::InputSnapshot snapshot;
        snapshot.keyboard = Microsoft::Xna::Framework::Input::KeyboardState({Keys::A, Keys::Q});
        Report("action_update", BestSeconds([&] { for (int i = 0; i < 10; ++i) small.Update(snapshot); }),
               BestSeconds([&] { for (int i = 0; i < 10; ++i) large.Update(snapshot); }));
    }
}
