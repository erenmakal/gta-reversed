/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/

#include "StdInc.h"

#include "LoadedCarGroup.h"

constexpr auto SENTINEL_VALUE_OF_UNUSED = (int16)(MODEL_INVALID);

void CLoadedCarGroup::InjectHooks() {
    RH_ScopedClass(CLoadedCarGroup);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(Clear, 0x611B90);
    RH_ScopedInstall(AddMember, 0x611BB0);
    RH_ScopedInstall(RemoveMember, 0x611BD0);
    RH_ScopedInstall(GetMember, 0x611C20);
    RH_ScopedInstall(CountMembers, 0x611C30);
    RH_ScopedInstall(PickRandomCar, 0x611C50);
    RH_ScopedInstall(SortBasedOnUsage, 0x611E10);
    RH_ScopedInstall(PickLeastUsedModel, 0x611E90);
}

// 0x611E10
void CLoadedCarGroup::SortBasedOnUsage() {
    // Sort from higher to lower usage
    rng::sort(GetAllModels(), std::greater<>{}, [](int16 modelid) {
        return CModelInfo::GetVehicleModelInfo(modelid)->m_nTimesUsed; }
    );
}

// 0x611BD0
void CLoadedCarGroup::RemoveMember(eModelID modelIndex) {
    if (notsa::remove_first(m_models, (int16)(modelIndex))) {
        m_models.back() = SENTINEL_VALUE_OF_UNUSED;
    }
}

// 0x611C50
eModelID CLoadedCarGroup::PickRandomCar(bool bNotTooManyInTheWorld, bool bOnlyPickNormalCars) {
    if (Empty()) {
        return MODEL_INVALID;
    }

    const auto PickRandom = [&](auto&& choices) {
        if (rng::empty(choices)) {
            return MODEL_INVALID;
        }

        // Vanilla only rejects a model after it has already been randomly selected
        // when there are more than two references to it. This still strongly favours
        // high-frequency models and is one of the reasons several identical cars can
        // appear together even while other suitable models are already loaded.
        //
        // Keep the original frequency weighting, but when the caller explicitly asks
        // for a model that isn't overrepresented, select from the least-referenced
        // eligible models first. This improves visible traffic variety without changing
        // CLoadedCarGroup's fixed ABI/layout or the streaming memory budget.
        int32 leastRefCount = std::numeric_limits<int32>::max();

        const auto IsBaseSuitable = [](eModelID model) {
            return !CTheScripts::HasCarModelBeenSuppressed(model)
                && !CTheScripts::HasVehicleModelBeenBlockedByScript(model)
                && !CStreaming::WeAreTryingToPhaseVehicleOut(model);
        };

        if (bNotTooManyInTheWorld) {
            for (const auto modelId : choices) {
                const auto model = (eModelID)(modelId);
                if (!IsBaseSuitable(model)) {
                    continue;
                }

                const auto refCount = (int32)(CModelInfo::GetVehicleModelInfo(model)->m_nRefCount);
                if (refCount <= 2) {
                    leastRefCount = std::min(leastRefCount, refCount);
                }
            }

            if (leastRefCount == std::numeric_limits<int32>::max()) {
                return MODEL_INVALID;
            }
        }

        const auto IsSuitable = [&](eModelID model) {
            if (!IsBaseSuitable(model)) {
                return false;
            }

            return !bNotTooManyInTheWorld
                || (int32)(CModelInfo::GetVehicleModelInfo(model)->m_nRefCount) == leastRefCount;
        };

        const auto weightSum = notsa::accumulate(choices, 0, [&](int16 modelId) {
            const auto model = (eModelID)(modelId);
            return IsSuitable(model)
                ? CModelInfo::GetVehicleModelInfo(model)->m_nFrq
                : 0;
        });

        if (weightSum <= 0) {
            return MODEL_INVALID;
        }

        auto pickedWeight = CGeneral::GetRandomNumberInRange(0, weightSum);
        eModelID lastSuitable = MODEL_INVALID;

        for (const auto modelId : choices) {
            const auto model = (eModelID)(modelId);
            if (!IsSuitable(model)) {
                continue;
            }

            lastSuitable = model;
            const auto thisModelFrq = CModelInfo::GetVehicleModelInfo(model)->m_nFrq;
            if (thisModelFrq >= pickedWeight) {
                return model;
            }
            pickedWeight -= thisModelFrq;
        }

        // Protect against rounding/range-edge differences while preserving a valid
        // weighted choice. Under normal circumstances the loop returns earlier.
        return lastSuitable;
    };

    if (bOnlyPickNormalCars) {
        // Originally an array was created here, and that was used
        // That is more performant, but I doubt that this is so performance critical to care about that.
        return PickRandom(
            GetAllModels() | rng::views::filter([](int16 modelId) {
                switch (CModelInfo::GetVehicleModelInfo(modelId)->m_nVehicleClass) {
                case VEHICLE_CLASS_NORMAL:
                case VEHICLE_CLASS_POORFAMILY:
                case VEHICLE_CLASS_RICHFAMILY:
                case VEHICLE_CLASS_MOTORBIKE:
                    return true;
                }
                return false;
            })
        );
    } else {
#ifdef FIX_BUGS
        return PickRandom(GetAllModels());
#else
        return PickRandom(m_models);
#endif
    }
}

// 0x611E90
eModelID CLoadedCarGroup::PickLeastUsedModel(int32 maxTimesUsed) {
    if (Empty()) {
        return MODEL_INVALID;
    }

    const auto GetMI = [](auto model) { return CModelInfo::GetVehicleModelInfo(model); };
    const auto ret = rng::min(GetAllModels(), [&](int16 modelA, int16 modelB) {
        const auto miA = GetMI(modelA), miB = GetMI(modelB);
        if (miA->m_nRefCount < miB->m_nRefCount) { // Primary sort criteria is `m_nRefCount`
            return true;
        }
        if (miA->m_nRefCount == miB->m_nRefCount) { // If that fails, secondary is `m_nTimesUsed`
            return miA->m_nTimesUsed < miB->m_nTimesUsed;
        }
        return false;
    });

    if (GetMI(ret)->m_nTimesUsed <= maxTimesUsed) {
        return (eModelID)(ret);
    }

    return MODEL_INVALID;
}

// 0x611C20
eModelID CLoadedCarGroup::GetMember(uint32 idx) const {
    assert(idx < CountMembers());
    return (eModelID)(m_models[idx]);
}

// 0x611C30
uint32 CLoadedCarGroup::CountMembers() const {
    return (uint32)(rng::distance(m_models.begin(), rng::find(m_models, SENTINEL_VALUE_OF_UNUSED)));
}

// NOTSA
bool CLoadedCarGroup::Empty() const {
    return m_models.front() == SENTINEL_VALUE_OF_UNUSED;
}

// 0x611B90
void CLoadedCarGroup::Clear() {
    rng::fill(m_models, SENTINEL_VALUE_OF_UNUSED);
}

// 0x611BB0
void CLoadedCarGroup::AddMember(eModelID member) {
    const auto end = rng::find(m_models, SENTINEL_VALUE_OF_UNUSED);
    if (end != m_models.end()) {
        *end = (int16)(member);
    } else {
        NOTSA_LOG_DEBUG("Failed to add model to group [Out of memory]");
    }
}
