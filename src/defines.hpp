#pragma once

#include <hyprutils/memory/WeakPtr.hpp>
#include <hyprutils/memory/UniquePtr.hpp>
#include <hyprutils/memory/SharedPtr.hpp>

using namespace Hyprutils::Memory;

#define SP CSharedPointer
#define WP CWeakPointer
#define UP CUniquePointer
