#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#define XR_USE_PLATFORM_WIN32
#define XR_USE_GRAPHICS_API_D3D11
#define XR_NO_PROTOTYPES
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include "area51xr/openxr_runtime.h"
#include "area51xr/xr_aim.h"

#include <array>
#include <cstring>
#include <string>
#include <vector>

namespace area51xr {
namespace {

template <typename T>
bool load_proc(PFN_xrGetInstanceProcAddr get_proc, XrInstance instance, const char* name, T& out) {
    PFN_xrVoidFunction fn{};
    if (XR_FAILED(get_proc(instance, name, &fn)) || !fn) {
        return false;
    }
    out = reinterpret_cast<T>(fn);
    return true;
}

bool luid_equal(const LUID& a, const LUID& b) {
    return a.HighPart == b.HighPart && a.LowPart == b.LowPart;
}

Pose to_pose(const XrPosef& pose) {
    Pose out{};
    out.position = {pose.position.x, pose.position.y, pose.position.z};
    out.orientation = {
        pose.orientation.x,
        pose.orientation.y,
        pose.orientation.z,
        pose.orientation.w
    };
    return out;
}

bool extension_available(
    const std::vector<XrExtensionProperties>& properties,
    const char* name) {
    for (const auto& property : properties) {
        if (std::strcmp(property.extensionName, name) == 0) {
            return true;
        }
    }
    return false;
}

constexpr const char* kTouchPlusExtension = "XR_META_touch_controller_plus";
constexpr const char* kTouchPlusProfile = "/interaction_profiles/meta/touch_controller_plus";

} // namespace

struct OpenXrRuntime::Impl {
    HMODULE loader{};
    XrInstance instance{XR_NULL_HANDLE};
    XrSystemId system{XR_NULL_SYSTEM_ID};
    XrSession session{XR_NULL_HANDLE};
    XrSpace local_space{XR_NULL_HANDLE};
    std::array<XrSpace, 2> aim_spaces{};
    XrActionSet action_set{XR_NULL_HANDLE};
    XrAction aim_action{XR_NULL_HANDLE};
    XrAction fire_action{XR_NULL_HANDLE};
    XrAction coin_action{XR_NULL_HANDLE};
    XrAction start_action{XR_NULL_HANDLE};
    XrAction menu_action{XR_NULL_HANDLE};
    std::array<XrPath, 2> hands{};
    XrPath touch_profile{XR_NULL_PATH};
    XrPath touch_plus_profile{XR_NULL_PATH};
    XrSwapchain quad_swapchain{XR_NULL_HANDLE};
    std::vector<XrSwapchainImageD3D11KHR> quad_images;
    std::uint32_t quad_width{};
    std::uint32_t quad_height{};
    XrSessionState session_state{XR_SESSION_STATE_UNKNOWN};
    XrTime predicted_display_time{};
    bool session_running{};
    bool frame_begun{};
    bool touch_plus_enabled{};
    bool fb_passthrough_detected{};
    bool fb_passthrough_enabled{};
    bool htc_passthrough_detected{};
    bool htc_passthrough_enabled{};
    bool passthrough_requested{};
    PassthroughState passthrough{PassthroughState::unavailable};
#ifdef XR_FB_passthrough
    XrPassthroughFB passthrough_handle{XR_NULL_HANDLE};
    XrPassthroughLayerFB passthrough_layer{XR_NULL_HANDLE};
#endif
#ifdef XR_HTC_passthrough
    XrPassthroughHTC htc_passthrough_handle{XR_NULL_HANDLE};
#endif
    std::size_t active_hand{1};
    std::array<bool, 2> previous_trigger{};
    std::array<bool, 2> previous_coin{};
    std::array<bool, 2> previous_start{};
    std::array<bool, 2> previous_menu{};
    std::uint64_t sample_number{};
    ID3D11Device* device{};
    ID3D11DeviceContext* context{};
    std::string error;

    PFN_xrGetInstanceProcAddr xrGetInstanceProcAddr{};
    PFN_xrCreateInstance xrCreateInstance{};
    PFN_xrDestroyInstance xrDestroyInstance{};
    PFN_xrGetSystem xrGetSystem{};
    PFN_xrGetD3D11GraphicsRequirementsKHR xrGetD3D11GraphicsRequirementsKHR{};
    PFN_xrCreateSession xrCreateSession{};
    PFN_xrDestroySession xrDestroySession{};
    PFN_xrPollEvent xrPollEvent{};
    PFN_xrBeginSession xrBeginSession{};
    PFN_xrEndSession xrEndSession{};
    PFN_xrCreateReferenceSpace xrCreateReferenceSpace{};
    PFN_xrDestroySpace xrDestroySpace{};
    PFN_xrCreateActionSet xrCreateActionSet{};
    PFN_xrDestroyActionSet xrDestroyActionSet{};
    PFN_xrCreateAction xrCreateAction{};
    PFN_xrStringToPath xrStringToPath{};
    PFN_xrSuggestInteractionProfileBindings xrSuggestInteractionProfileBindings{};
    PFN_xrAttachSessionActionSets xrAttachSessionActionSets{};
    PFN_xrCreateActionSpace xrCreateActionSpace{};
    PFN_xrSyncActions xrSyncActions{};
    PFN_xrGetActionStateBoolean xrGetActionStateBoolean{};
    PFN_xrGetActionStatePose xrGetActionStatePose{};
    PFN_xrGetCurrentInteractionProfile xrGetCurrentInteractionProfile{};
    PFN_xrLocateSpace xrLocateSpace{};
    PFN_xrWaitFrame xrWaitFrame{};
    PFN_xrBeginFrame xrBeginFrame{};
    PFN_xrEndFrame xrEndFrame{};
    PFN_xrEnumerateSwapchainFormats xrEnumerateSwapchainFormats{};
    PFN_xrCreateSwapchain xrCreateSwapchain{};
    PFN_xrDestroySwapchain xrDestroySwapchain{};
    PFN_xrEnumerateSwapchainImages xrEnumerateSwapchainImages{};
    PFN_xrAcquireSwapchainImage xrAcquireSwapchainImage{};
    PFN_xrWaitSwapchainImage xrWaitSwapchainImage{};
    PFN_xrReleaseSwapchainImage xrReleaseSwapchainImage{};
#ifdef XR_FB_passthrough
    PFN_xrCreatePassthroughFB xrCreatePassthroughFB{};
    PFN_xrDestroyPassthroughFB xrDestroyPassthroughFB{};
    PFN_xrPassthroughStartFB xrPassthroughStartFB{};
    PFN_xrPassthroughPauseFB xrPassthroughPauseFB{};
    PFN_xrCreatePassthroughLayerFB xrCreatePassthroughLayerFB{};
    PFN_xrDestroyPassthroughLayerFB xrDestroyPassthroughLayerFB{};
    PFN_xrPassthroughLayerResumeFB xrPassthroughLayerResumeFB{};
    PFN_xrPassthroughLayerPauseFB xrPassthroughLayerPauseFB{};
#endif
#ifdef XR_HTC_passthrough
    PFN_xrCreatePassthroughHTC xrCreatePassthroughHTC{};
    PFN_xrDestroyPassthroughHTC xrDestroyPassthroughHTC{};
#endif

    bool fail(const char* text) {
        error = text;
        return false;
    }

    bool load_functions() {
        return
            load_proc(xrGetInstanceProcAddr, instance, "xrDestroyInstance", xrDestroyInstance) &&
            load_proc(xrGetInstanceProcAddr, instance, "xrGetSystem", xrGetSystem) &&
            load_proc(xrGetInstanceProcAddr, instance, "xrGetD3D11GraphicsRequirementsKHR", xrGetD3D11GraphicsRequirementsKHR) &&
            load_proc(xrGetInstanceProcAddr, instance, "xrCreateSession", xrCreateSession) &&
            load_proc(xrGetInstanceProcAddr, instance, "xrDestroySession", xrDestroySession) &&
            load_proc(xrGetInstanceProcAddr, instance, "xrPollEvent", xrPollEvent) &&
            load_proc(xrGetInstanceProcAddr, instance, "xrBeginSession", xrBeginSession) &&
            load_proc(xrGetInstanceProcAddr, instance, "xrEndSession", xrEndSession) &&
            load_proc(xrGetInstanceProcAddr, instance, "xrCreateReferenceSpace", xrCreateReferenceSpace) &&
            load_proc(xrGetInstanceProcAddr, instance, "xrDestroySpace", xrDestroySpace) &&
            load_proc(xrGetInstanceProcAddr, instance, "xrCreateActionSet", xrCreateActionSet) &&
            load_proc(xrGetInstanceProcAddr, instance, "xrDestroyActionSet", xrDestroyActionSet) &&
            load_proc(xrGetInstanceProcAddr, instance, "xrCreateAction", xrCreateAction) &&
            load_proc(xrGetInstanceProcAddr, instance, "xrStringToPath", xrStringToPath) &&
            load_proc(xrGetInstanceProcAddr, instance, "xrSuggestInteractionProfileBindings", xrSuggestInteractionProfileBindings) &&
            load_proc(xrGetInstanceProcAddr, instance, "xrAttachSessionActionSets", xrAttachSessionActionSets) &&
            load_proc(xrGetInstanceProcAddr, instance, "xrCreateActionSpace", xrCreateActionSpace) &&
            load_proc(xrGetInstanceProcAddr, instance, "xrSyncActions", xrSyncActions) &&
            load_proc(xrGetInstanceProcAddr, instance, "xrGetActionStateBoolean", xrGetActionStateBoolean) &&
            load_proc(xrGetInstanceProcAddr, instance, "xrGetActionStatePose", xrGetActionStatePose) &&
            load_proc(xrGetInstanceProcAddr, instance, "xrGetCurrentInteractionProfile", xrGetCurrentInteractionProfile) &&
            load_proc(xrGetInstanceProcAddr, instance, "xrLocateSpace", xrLocateSpace) &&
            load_proc(xrGetInstanceProcAddr, instance, "xrWaitFrame", xrWaitFrame) &&
            load_proc(xrGetInstanceProcAddr, instance, "xrBeginFrame", xrBeginFrame) &&
            load_proc(xrGetInstanceProcAddr, instance, "xrEndFrame", xrEndFrame) &&
            load_proc(xrGetInstanceProcAddr, instance, "xrEnumerateSwapchainFormats", xrEnumerateSwapchainFormats) &&
            load_proc(xrGetInstanceProcAddr, instance, "xrCreateSwapchain", xrCreateSwapchain) &&
            load_proc(xrGetInstanceProcAddr, instance, "xrDestroySwapchain", xrDestroySwapchain) &&
            load_proc(xrGetInstanceProcAddr, instance, "xrEnumerateSwapchainImages", xrEnumerateSwapchainImages) &&
            load_proc(xrGetInstanceProcAddr, instance, "xrAcquireSwapchainImage", xrAcquireSwapchainImage) &&
            load_proc(xrGetInstanceProcAddr, instance, "xrWaitSwapchainImage", xrWaitSwapchainImage) &&
            load_proc(xrGetInstanceProcAddr, instance, "xrReleaseSwapchainImage", xrReleaseSwapchainImage);
    }

    bool create_passthrough_resources() {
#ifdef XR_FB_passthrough
        if (fb_passthrough_enabled) {
            const bool loaded =
                load_proc(xrGetInstanceProcAddr, instance, "xrCreatePassthroughFB", xrCreatePassthroughFB) &&
                load_proc(xrGetInstanceProcAddr, instance, "xrDestroyPassthroughFB", xrDestroyPassthroughFB) &&
                load_proc(xrGetInstanceProcAddr, instance, "xrPassthroughStartFB", xrPassthroughStartFB) &&
                load_proc(xrGetInstanceProcAddr, instance, "xrPassthroughPauseFB", xrPassthroughPauseFB) &&
                load_proc(xrGetInstanceProcAddr, instance, "xrCreatePassthroughLayerFB", xrCreatePassthroughLayerFB) &&
                load_proc(xrGetInstanceProcAddr, instance, "xrDestroyPassthroughLayerFB", xrDestroyPassthroughLayerFB) &&
                load_proc(xrGetInstanceProcAddr, instance, "xrPassthroughLayerResumeFB", xrPassthroughLayerResumeFB) &&
                load_proc(xrGetInstanceProcAddr, instance, "xrPassthroughLayerPauseFB", xrPassthroughLayerPauseFB);
            if (loaded) {
                XrPassthroughCreateInfoFB passthrough_info{XR_TYPE_PASSTHROUGH_CREATE_INFO_FB};
                if (XR_SUCCEEDED(xrCreatePassthroughFB(session, &passthrough_info, &passthrough_handle))) {
                    XrPassthroughLayerCreateInfoFB layer_info{XR_TYPE_PASSTHROUGH_LAYER_CREATE_INFO_FB};
                    layer_info.passthrough = passthrough_handle;
                    layer_info.purpose = XR_PASSTHROUGH_LAYER_PURPOSE_RECONSTRUCTION_FB;
                    if (XR_SUCCEEDED(xrCreatePassthroughLayerFB(
                            session, &layer_info, &passthrough_layer))) {
                        passthrough = PassthroughState::off;
                        return true;
                    }
                    xrDestroyPassthroughFB(passthrough_handle);
                    passthrough_handle = XR_NULL_HANDLE;
                }
            }
            fb_passthrough_enabled = false;
        }
#endif
#ifdef XR_HTC_passthrough
        if (htc_passthrough_enabled &&
            load_proc(xrGetInstanceProcAddr, instance, "xrCreatePassthroughHTC", xrCreatePassthroughHTC) &&
            load_proc(xrGetInstanceProcAddr, instance, "xrDestroyPassthroughHTC", xrDestroyPassthroughHTC)) {
            XrPassthroughCreateInfoHTC info{XR_TYPE_PASSTHROUGH_CREATE_INFO_HTC};
            info.form = XR_PASSTHROUGH_FORM_PLANAR_HTC;
            if (XR_SUCCEEDED(xrCreatePassthroughHTC(session, &info, &htc_passthrough_handle))) {
                passthrough = PassthroughState::off;
                return true;
            }
        }
        htc_passthrough_enabled = false;
#endif
        return false;
    }

    bool set_passthrough(bool enabled) {
        if (passthrough == PassthroughState::unavailable) return false;
        passthrough_requested = enabled;
        if (!session_running) {
            passthrough = PassthroughState::off;
            return true;
        }
#ifdef XR_FB_passthrough
        if (fb_passthrough_enabled && passthrough_handle && passthrough_layer) {
            if (enabled) {
                if (XR_FAILED(xrPassthroughStartFB(passthrough_handle)) ||
                    XR_FAILED(xrPassthroughLayerResumeFB(passthrough_layer))) {
                    passthrough = PassthroughState::unavailable;
                    fb_passthrough_enabled = false;
                    return false;
                }
                passthrough = PassthroughState::on;
            } else {
                xrPassthroughLayerPauseFB(passthrough_layer);
                xrPassthroughPauseFB(passthrough_handle);
                passthrough = PassthroughState::off;
            }
            return true;
        }
#endif
#ifdef XR_HTC_passthrough
        if (htc_passthrough_enabled && htc_passthrough_handle) {
            passthrough = enabled ? PassthroughState::on : PassthroughState::off;
            return true;
        }
#endif
        passthrough = PassthroughState::unavailable;
        return false;
    }

    bool create_device(const LUID& required_luid, D3D_FEATURE_LEVEL min_feature) {
        IDXGIFactory1* factory{};
        if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) {
            return fail("CreateDXGIFactory1 failed");
        }

        IDXGIAdapter1* selected{};
        for (UINT index = 0; ; ++index) {
            IDXGIAdapter1* adapter{};
            if (factory->EnumAdapters1(index, &adapter) == DXGI_ERROR_NOT_FOUND) {
                break;
            }
            DXGI_ADAPTER_DESC1 desc{};
            adapter->GetDesc1(&desc);
            if (luid_equal(desc.AdapterLuid, required_luid)) {
                selected = adapter;
                break;
            }
            adapter->Release();
        }
        factory->Release();

        if (!selected) {
            return fail("OpenXR graphics adapter was not found");
        }

        const std::array<D3D_FEATURE_LEVEL, 6> levels{
            D3D_FEATURE_LEVEL_11_1,
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_1,
            D3D_FEATURE_LEVEL_10_0,
            D3D_FEATURE_LEVEL_9_3,
            D3D_FEATURE_LEVEL_9_1
        };
        D3D_FEATURE_LEVEL created{};
        const HRESULT hr = D3D11CreateDevice(
            selected,
            D3D_DRIVER_TYPE_UNKNOWN,
            nullptr,
            0,
            levels.data(),
            static_cast<UINT>(levels.size()),
            D3D11_SDK_VERSION,
            &device,
            &created,
            &context);
        selected->Release();

        if (FAILED(hr) || created < min_feature) {
            return fail("D3D11 device creation did not meet OpenXR requirements");
        }
        return true;
    }

    bool make_path(const char* value, XrPath& path) {
        return XR_SUCCEEDED(xrStringToPath(instance, value, &path));
    }

    bool suggest_bindings(
        const char* profile_path,
        const char* left_fire_path,
        const char* right_fire_path,
        bool include_full_controls,
        XrPath& bound_profile) {
        XrPath profile{};
        if (!make_path(profile_path, profile)) {
            return false;
        }

        std::vector<XrActionSuggestedBinding> bindings;
        bindings.reserve(include_full_controls ? 10 : 4);
        auto add = [&](XrAction action, const char* path_text) {
            XrPath path{};
            if (!make_path(path_text, path)) {
                return false;
            }
            bindings.push_back({action, path});
            return true;
        };

        if (!add(aim_action, "/user/hand/left/input/aim/pose") ||
            !add(aim_action, "/user/hand/right/input/aim/pose") ||
            !add(fire_action, left_fire_path) ||
            !add(fire_action, right_fire_path)) {
            return false;
        }

        if (include_full_controls) {
            if (!add(start_action, "/user/hand/left/input/x/click") ||
                !add(start_action, "/user/hand/right/input/a/click") ||
                !add(coin_action, "/user/hand/left/input/y/click") ||
                !add(coin_action, "/user/hand/right/input/b/click") ||
                !add(menu_action, "/user/hand/left/input/thumbstick/click") ||
                !add(menu_action, "/user/hand/right/input/thumbstick/click")) {
                return false;
            }
        }

        XrInteractionProfileSuggestedBinding suggested{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
        suggested.interactionProfile = profile;
        suggested.countSuggestedBindings = static_cast<std::uint32_t>(bindings.size());
        suggested.suggestedBindings = bindings.data();
        if (XR_FAILED(xrSuggestInteractionProfileBindings(instance, &suggested))) {
            return false;
        }
        if (include_full_controls) {
            bound_profile = profile;
        }
        return true;
    }

    bool create_boolean_action(const char* name, const char* localized_name, XrAction& action) {
        XrActionCreateInfo info{XR_TYPE_ACTION_CREATE_INFO};
        info.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
        std::strncpy(info.actionName, name, XR_MAX_ACTION_NAME_SIZE - 1);
        std::strncpy(info.localizedActionName, localized_name, XR_MAX_LOCALIZED_ACTION_NAME_SIZE - 1);
        info.countSubactionPaths = static_cast<std::uint32_t>(hands.size());
        info.subactionPaths = hands.data();
        return XR_SUCCEEDED(xrCreateAction(action_set, &info, &action));
    }

    bool create_actions() {
        XrActionSetCreateInfo set_info{XR_TYPE_ACTION_SET_CREATE_INFO};
        std::strncpy(set_info.actionSetName, "gameplay", XR_MAX_ACTION_SET_NAME_SIZE - 1);
        std::strncpy(set_info.localizedActionSetName, "Gameplay", XR_MAX_LOCALIZED_ACTION_SET_NAME_SIZE - 1);
        if (XR_FAILED(xrCreateActionSet(instance, &set_info, &action_set))) {
            return fail("xrCreateActionSet failed");
        }

        if (!make_path("/user/hand/left", hands[0]) ||
            !make_path("/user/hand/right", hands[1])) {
            return fail("controller hand paths unavailable");
        }

        XrActionCreateInfo aim_info{XR_TYPE_ACTION_CREATE_INFO};
        aim_info.actionType = XR_ACTION_TYPE_POSE_INPUT;
        std::strncpy(aim_info.actionName, "aim_pose", XR_MAX_ACTION_NAME_SIZE - 1);
        std::strncpy(aim_info.localizedActionName, "Aim Pose", XR_MAX_LOCALIZED_ACTION_NAME_SIZE - 1);
        aim_info.countSubactionPaths = static_cast<std::uint32_t>(hands.size());
        aim_info.subactionPaths = hands.data();
        if (XR_FAILED(xrCreateAction(action_set, &aim_info, &aim_action))) {
            return fail("aim action creation failed");
        }

        if (!create_boolean_action("fire", "Fire", fire_action) ||
            !create_boolean_action("coin", "Insert Coin", coin_action) ||
            !create_boolean_action("start", "Start Continue", start_action) ||
            !create_boolean_action("pause_menu", "Pause Menu", menu_action)) {
            return fail("controller action creation failed");
        }

        XrPath ignored_profile{XR_NULL_PATH};
        const bool simple_ok = suggest_bindings(
            "/interaction_profiles/khr/simple_controller",
            "/user/hand/left/input/select/click",
            "/user/hand/right/input/select/click",
            false,
            ignored_profile);
        const bool touch_ok = suggest_bindings(
            "/interaction_profiles/oculus/touch_controller",
            "/user/hand/left/input/trigger/value",
            "/user/hand/right/input/trigger/value",
            true,
            touch_profile);
        const bool touch_plus_ok = touch_plus_enabled && suggest_bindings(
            kTouchPlusProfile,
            "/user/hand/left/input/trigger/value",
            "/user/hand/right/input/trigger/value",
            true,
            touch_plus_profile);

        if (!simple_ok && !touch_ok && !touch_plus_ok) {
            return fail("no supported controller bindings could be suggested");
        }
        return true;
    }

    bool active_profile_has_full_controls(std::size_t hand) const {
        if (!xrGetCurrentInteractionProfile || !session || !hands[hand]) {
            return false;
        }
        XrInteractionProfileState profile{XR_TYPE_INTERACTION_PROFILE_STATE};
        if (XR_FAILED(xrGetCurrentInteractionProfile(session, hands[hand], &profile))) {
            return false;
        }
        return
            (touch_profile != XR_NULL_PATH && profile.interactionProfile == touch_profile) ||
            (touch_plus_profile != XR_NULL_PATH && profile.interactionProfile == touch_plus_profile);
    }

    bool read_boolean_action(XrAction action, std::size_t hand, bool& down) {
        down = false;
        if (!action) {
            return true;
        }
        XrActionStateGetInfo get{XR_TYPE_ACTION_STATE_GET_INFO};
        get.action = action;
        get.subactionPath = hands[hand];
        XrActionStateBoolean state{XR_TYPE_ACTION_STATE_BOOLEAN};
        if (XR_FAILED(xrGetActionStateBoolean(session, &get, &state))) {
            return false;
        }
        down = state.isActive && state.currentState == XR_TRUE;
        return true;
    }

    bool create_quad_swapchain(std::uint32_t width, std::uint32_t height) {
        if (quad_swapchain && width == quad_width && height == quad_height) {
            return true;
        }
        if (quad_swapchain) {
            xrDestroySwapchain(quad_swapchain);
            quad_swapchain = XR_NULL_HANDLE;
            quad_images.clear();
        }

        std::uint32_t count{};
        if (XR_FAILED(xrEnumerateSwapchainFormats(session, 0, &count, nullptr)) || count == 0) {
            return fail("no OpenXR swapchain formats available");
        }
        std::vector<std::int64_t> formats(count);
        if (XR_FAILED(xrEnumerateSwapchainFormats(session, count, &count, formats.data()))) {
            return fail("xrEnumerateSwapchainFormats failed");
        }

        std::int64_t format = 0;
        for (const auto candidate : formats) {
            if (candidate == DXGI_FORMAT_B8G8R8A8_UNORM || candidate == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB) {
                format = candidate;
                break;
            }
        }
        if (!format) {
            return fail("runtime does not expose a BGRA8 swapchain format");
        }

        XrSwapchainCreateInfo info{XR_TYPE_SWAPCHAIN_CREATE_INFO};
        info.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
        info.format = format;
        info.sampleCount = 1;
        info.width = width;
        info.height = height;
        info.faceCount = 1;
        info.arraySize = 1;
        info.mipCount = 1;
        if (XR_FAILED(xrCreateSwapchain(session, &info, &quad_swapchain))) {
            return fail("xrCreateSwapchain failed");
        }

        std::uint32_t image_count{};
        if (XR_FAILED(xrEnumerateSwapchainImages(quad_swapchain, 0, &image_count, nullptr)) || image_count == 0) {
            return fail("OpenXR swapchain contains no images");
        }
        quad_images.assign(image_count, XrSwapchainImageD3D11KHR{XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR});
        if (XR_FAILED(xrEnumerateSwapchainImages(
                quad_swapchain,
                image_count,
                &image_count,
                reinterpret_cast<XrSwapchainImageBaseHeader*>(quad_images.data())))) {
            return fail("xrEnumerateSwapchainImages failed");
        }

        quad_width = width;
        quad_height = height;
        return true;
    }

    bool begin_if_ready(XrSessionState next) {
        session_state = next;
        if (next == XR_SESSION_STATE_READY && !session_running) {
            XrSessionBeginInfo begin{XR_TYPE_SESSION_BEGIN_INFO};
            begin.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
            if (XR_FAILED(xrBeginSession(session, &begin))) {
                return fail("xrBeginSession failed");
            }
            session_running = true;
            if (passthrough_requested && passthrough != PassthroughState::unavailable)
                set_passthrough(true);
        } else if (next == XR_SESSION_STATE_STOPPING && session_running) {
#ifdef XR_FB_passthrough
            if (passthrough == PassthroughState::on) {
                xrPassthroughLayerPauseFB(passthrough_layer);
                xrPassthroughPauseFB(passthrough_handle);
                passthrough = PassthroughState::off;
            }
#endif
            if (passthrough == PassthroughState::on)
                passthrough = PassthroughState::off;
            xrEndSession(session);
            session_running = false;
            frame_begun = false;
        }
        return true;
    }

    bool poll_events() {
        XrEventDataBuffer event{XR_TYPE_EVENT_DATA_BUFFER};
        while (xrPollEvent(instance, &event) == XR_SUCCESS) {
            if (event.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
                const auto* changed = reinterpret_cast<const XrEventDataSessionStateChanged*>(&event);
                if (!begin_if_ready(changed->state)) {
                    return false;
                }
            } else if (event.type == XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING) {
                return fail("OpenXR instance loss pending");
            }
            event = XrEventDataBuffer{XR_TYPE_EVENT_DATA_BUFFER};
        }
        return true;
    }
};

OpenXrRuntime::OpenXrRuntime() : impl_(std::make_unique<Impl>()) {}
OpenXrRuntime::~OpenXrRuntime() { shutdown(); }

bool OpenXrRuntime::initialize() {
    auto& p = *impl_;
    p.error.clear();
    p.loader = LoadLibraryW(L"openxr_loader.dll");
    if (!p.loader) {
        return p.fail("openxr_loader.dll was not found");
    }

    p.xrGetInstanceProcAddr = reinterpret_cast<PFN_xrGetInstanceProcAddr>(
        GetProcAddress(p.loader, "xrGetInstanceProcAddr"));
    if (!p.xrGetInstanceProcAddr ||
        !load_proc(p.xrGetInstanceProcAddr, XR_NULL_HANDLE, "xrCreateInstance", p.xrCreateInstance)) {
        return p.fail("OpenXR loader entry points unavailable");
    }

    PFN_xrEnumerateInstanceExtensionProperties enumerate_extensions{};
    if (!load_proc(
            p.xrGetInstanceProcAddr,
            XR_NULL_HANDLE,
            "xrEnumerateInstanceExtensionProperties",
            enumerate_extensions)) {
        return p.fail("xrEnumerateInstanceExtensionProperties unavailable");
    }

    std::uint32_t extension_count{};
    if (XR_FAILED(enumerate_extensions(nullptr, 0, &extension_count, nullptr))) {
        return p.fail("OpenXR extension enumeration failed");
    }
    std::vector<XrExtensionProperties> available_extensions(extension_count);
    for (auto& extension : available_extensions) {
        extension.type = XR_TYPE_EXTENSION_PROPERTIES;
        extension.next = nullptr;
    }
    if (extension_count != 0 && XR_FAILED(enumerate_extensions(
            nullptr,
            extension_count,
            &extension_count,
            available_extensions.data()))) {
        return p.fail("OpenXR extension enumeration failed");
    }

    p.touch_plus_enabled = extension_available(available_extensions, kTouchPlusExtension);
#ifdef XR_FB_passthrough
    p.fb_passthrough_detected = extension_available(
        available_extensions, XR_FB_PASSTHROUGH_EXTENSION_NAME);
    p.fb_passthrough_enabled = p.fb_passthrough_detected;
#endif
#ifdef XR_HTC_passthrough
    p.htc_passthrough_detected = extension_available(
        available_extensions, XR_HTC_PASSTHROUGH_EXTENSION_NAME);
    p.htc_passthrough_enabled = p.htc_passthrough_detected;
#endif

    std::vector<const char*> extensions;
    extensions.push_back(XR_KHR_D3D11_ENABLE_EXTENSION_NAME);
    if (p.touch_plus_enabled) {
        extensions.push_back(kTouchPlusExtension);
    }
#ifdef XR_FB_passthrough
    if (p.fb_passthrough_enabled) {
        extensions.push_back(XR_FB_PASSTHROUGH_EXTENSION_NAME);
    }
#endif
#ifdef XR_HTC_passthrough
    if (p.htc_passthrough_enabled) {
        extensions.push_back(XR_HTC_PASSTHROUGH_EXTENSION_NAME);
    }
#endif

    XrInstanceCreateInfo instance_info{XR_TYPE_INSTANCE_CREATE_INFO};
    std::strncpy(instance_info.applicationInfo.applicationName, "Area51XR", XR_MAX_APPLICATION_NAME_SIZE - 1);
    instance_info.applicationInfo.applicationVersion = 2;
    std::strncpy(instance_info.applicationInfo.engineName, "Area51XR", XR_MAX_ENGINE_NAME_SIZE - 1);
    instance_info.applicationInfo.engineVersion = 2;
    instance_info.applicationInfo.apiVersion = XR_API_VERSION_1_0;
    instance_info.enabledExtensionCount = static_cast<std::uint32_t>(extensions.size());
    instance_info.enabledExtensionNames = extensions.data();
    if (XR_FAILED(p.xrCreateInstance(&instance_info, &p.instance))) {
        return p.fail("xrCreateInstance failed");
    }
    if (!p.load_functions()) {
        return p.fail("required OpenXR functions unavailable");
    }

    XrSystemGetInfo system_info{XR_TYPE_SYSTEM_GET_INFO};
    system_info.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    if (XR_FAILED(p.xrGetSystem(p.instance, &system_info, &p.system))) {
        return p.fail("no OpenXR HMD system is available");
    }

    XrGraphicsRequirementsD3D11KHR graphics{XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR};
    if (XR_FAILED(p.xrGetD3D11GraphicsRequirementsKHR(p.instance, p.system, &graphics)) ||
        !p.create_device(graphics.adapterLuid, graphics.minFeatureLevel)) {
        return false;
    }

    XrGraphicsBindingD3D11KHR binding{XR_TYPE_GRAPHICS_BINDING_D3D11_KHR};
    binding.device = p.device;
    XrSessionCreateInfo session_info{XR_TYPE_SESSION_CREATE_INFO};
    session_info.next = &binding;
    session_info.systemId = p.system;
    if (XR_FAILED(p.xrCreateSession(p.instance, &session_info, &p.session))) {
        return p.fail("xrCreateSession failed");
    }
    p.create_passthrough_resources();

    XrReferenceSpaceCreateInfo space_info{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
    space_info.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
    space_info.poseInReferenceSpace.orientation.w = 1.0f;
    if (XR_FAILED(p.xrCreateReferenceSpace(p.session, &space_info, &p.local_space))) {
        return p.fail("local reference space creation failed");
    }

    if (!p.create_actions()) {
        return false;
    }
    XrSessionActionSetsAttachInfo attach{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
    attach.countActionSets = 1;
    attach.actionSets = &p.action_set;
    if (XR_FAILED(p.xrAttachSessionActionSets(p.session, &attach))) {
        return p.fail("xrAttachSessionActionSets failed");
    }

    for (std::size_t hand = 0; hand < p.hands.size(); ++hand) {
        XrActionSpaceCreateInfo aim_space_info{XR_TYPE_ACTION_SPACE_CREATE_INFO};
        aim_space_info.action = p.aim_action;
        aim_space_info.subactionPath = p.hands[hand];
        aim_space_info.poseInActionSpace.orientation.w = 1.0f;
        if (XR_FAILED(p.xrCreateActionSpace(p.session, &aim_space_info, &p.aim_spaces[hand]))) {
            return p.fail("aim action space creation failed");
        }
    }
    return true;
}

bool OpenXrRuntime::poll(XrInputState& state) {
    auto& p = *impl_;
    state = {};
    state.sample_number = ++p.sample_number;
    if (!p.instance || !p.poll_events()) {
        return false;
    }

    state.session_running = p.session_running;
    if (!p.session_running) {
        return true;
    }

    XrFrameWaitInfo wait_info{XR_TYPE_FRAME_WAIT_INFO};
    XrFrameState frame_state{XR_TYPE_FRAME_STATE};
    if (XR_FAILED(p.xrWaitFrame(p.session, &wait_info, &frame_state))) {
        return p.fail("xrWaitFrame failed");
    }
    p.predicted_display_time = frame_state.predictedDisplayTime;

    XrFrameBeginInfo begin_info{XR_TYPE_FRAME_BEGIN_INFO};
    if (XR_FAILED(p.xrBeginFrame(p.session, &begin_info))) {
        return p.fail("xrBeginFrame failed");
    }
    p.frame_begun = true;

    XrActiveActionSet active{p.action_set, XR_NULL_PATH};
    XrActionsSyncInfo sync{XR_TYPE_ACTIONS_SYNC_INFO};
    sync.countActiveActionSets = 1;
    sync.activeActionSets = &active;
    if (XR_FAILED(p.xrSyncActions(p.session, &sync))) {
        return p.fail("xrSyncActions failed");
    }

    std::array<bool, 2> pose_valid{};
    std::array<Pose, 2> poses{};
    std::array<bool, 2> trigger{};
    std::array<bool, 2> coin{};
    std::array<bool, 2> start{};
    std::array<bool, 2> menu{};

    for (std::size_t hand = 0; hand < p.hands.size(); ++hand) {
        XrActionStateGetInfo aim_get{XR_TYPE_ACTION_STATE_GET_INFO};
        aim_get.action = p.aim_action;
        aim_get.subactionPath = p.hands[hand];
        XrActionStatePose aim_state{XR_TYPE_ACTION_STATE_POSE};
        if (XR_SUCCEEDED(p.xrGetActionStatePose(p.session, &aim_get, &aim_state)) && aim_state.isActive) {
            XrSpaceLocation location{XR_TYPE_SPACE_LOCATION};
            if (XR_SUCCEEDED(p.xrLocateSpace(
                    p.aim_spaces[hand], p.local_space, p.predicted_display_time, &location))) {
                const XrSpaceLocationFlags required =
                    XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
                if ((location.locationFlags & required) == required) {
                    pose_valid[hand] = true;
                    poses[hand] = to_pose(location.pose);
                }
            }
        }

        if (!p.read_boolean_action(p.fire_action, hand, trigger[hand])) {
            return p.fail("fire action state unavailable");
        }
        if (p.active_profile_has_full_controls(hand)) {
            if (!p.read_boolean_action(p.coin_action, hand, coin[hand]) ||
                !p.read_boolean_action(p.start_action, hand, start[hand]) ||
                !p.read_boolean_action(p.menu_action, hand, menu[hand])) {
                return p.fail("controller action state unavailable");
            }
        }
    }

    const bool left_activity =
        (trigger[0] && !p.previous_trigger[0]) ||
        (coin[0] && !p.previous_coin[0]) ||
        (start[0] && !p.previous_start[0]) ||
        (menu[0] && !p.previous_menu[0]);
    const bool right_activity =
        (trigger[1] && !p.previous_trigger[1]) ||
        (coin[1] && !p.previous_coin[1]) ||
        (start[1] && !p.previous_start[1]) ||
        (menu[1] && !p.previous_menu[1]);
    if (left_activity != right_activity) {
        p.active_hand = left_activity ? 0u : 1u;
    }
    if (!pose_valid[p.active_hand] && pose_valid[1u - p.active_hand]) {
        p.active_hand = 1u - p.active_hand;
    }

    state.left_pose_valid = pose_valid[0];
    state.left_aim = poses[0];
    state.left_trigger_down = trigger[0];
    state.left_coin_down = coin[0];
    state.left_start_down = start[0];
    state.left_menu_down = menu[0];
    state.right_pose_valid = pose_valid[1];
    state.right_aim = poses[1];
    state.right_trigger_down = trigger[1];
    state.right_coin_down = coin[1];
    state.right_start_down = start[1];
    state.right_menu_down = menu[1];

    state.active_hand = p.active_hand == 0 ? ControllerHand::left : ControllerHand::right;
    state.pose_valid = pose_valid[p.active_hand];
    state.aim = poses[p.active_hand];
    state.trigger_down = trigger[p.active_hand];
    state.coin_down = coin[0] || coin[1];
    state.start_down = start[0] || start[1];
    state.menu_down = menu[0] || menu[1];

    p.previous_trigger = trigger;
    p.previous_coin = coin;
    p.previous_start = start;
    p.previous_menu = menu;
    return true;
}

bool OpenXrRuntime::present(const VideoFrameView& frame) {
    auto& p = *impl_;
    if (!p.session_running) {
        return true;
    }
    if (!p.frame_begun) {
        return p.fail("OpenXR frame was not begun before presentation");
    }

    const XrCompositionLayerBaseHeader* layers[2]{};
    std::uint32_t layer_count = 0;
    XrCompositionLayerQuad quad{XR_TYPE_COMPOSITION_LAYER_QUAD};
#ifdef XR_FB_passthrough
    XrCompositionLayerPassthroughFB passthrough_layer{XR_TYPE_COMPOSITION_LAYER_PASSTHROUGH_FB};
    if (p.passthrough == PassthroughState::on && p.passthrough_layer) {
        passthrough_layer.layerHandle = p.passthrough_layer;
        layers[layer_count++] = reinterpret_cast<const XrCompositionLayerBaseHeader*>(&passthrough_layer);
    }
#endif
#ifdef XR_HTC_passthrough
    XrCompositionLayerPassthroughHTC htc_layer{XR_TYPE_COMPOSITION_LAYER_PASSTHROUGH_HTC};
    if (p.passthrough == PassthroughState::on && p.htc_passthrough_handle) {
        htc_layer.space = p.local_space;
        htc_layer.passthrough = p.htc_passthrough_handle;
        htc_layer.color = XrPassthroughColorHTC{XR_TYPE_PASSTHROUGH_COLOR_HTC};
        htc_layer.color.alpha = 1.0f;
        layers[layer_count++] = reinterpret_cast<const XrCompositionLayerBaseHeader*>(&htc_layer);
    }
#endif

    if (frame.width && frame.height && !frame.pixels.empty()) {
        if (frame.pixel_format != 1 || frame.stride_bytes < frame.width * 4u) {
            return p.fail("unsupported MAME framebuffer format");
        }
        if (!p.create_quad_swapchain(frame.width, frame.height)) {
            return false;
        }

        std::uint32_t image_index{};
        XrSwapchainImageAcquireInfo acquire{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
        if (XR_FAILED(p.xrAcquireSwapchainImage(p.quad_swapchain, &acquire, &image_index))) {
            return p.fail("xrAcquireSwapchainImage failed");
        }
        XrSwapchainImageWaitInfo wait{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
        wait.timeout = XR_INFINITE_DURATION;
        if (XR_FAILED(p.xrWaitSwapchainImage(p.quad_swapchain, &wait))) {
            return p.fail("xrWaitSwapchainImage failed");
        }

        p.context->UpdateSubresource(
            p.quad_images[image_index].texture,
            0,
            nullptr,
            frame.pixels.data(),
            frame.stride_bytes,
            0);

        XrSwapchainImageReleaseInfo release{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
        if (XR_FAILED(p.xrReleaseSwapchainImage(p.quad_swapchain, &release))) {
            return p.fail("xrReleaseSwapchainImage failed");
        }

        quad.layerFlags = 0;
        quad.space = p.local_space;
        quad.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
        quad.subImage.swapchain = p.quad_swapchain;
        quad.subImage.imageRect.offset = {0, 0};
        quad.subImage.imageRect.extent = {
            static_cast<std::int32_t>(frame.width),
            static_cast<std::int32_t>(frame.height)
        };
        quad.subImage.imageArrayIndex = 0;
        quad.pose.orientation.w = 1.0f;
        quad.pose.position = {0.0f, 0.0f, -kDefaultScreenDistanceMeters};
        quad.size = {
            kDefaultScreenWidthMeters,
            kDefaultScreenWidthMeters * static_cast<float>(frame.height) / static_cast<float>(frame.width)
        };
        layers[layer_count++] = reinterpret_cast<const XrCompositionLayerBaseHeader*>(&quad);
    }

    XrFrameEndInfo end_info{XR_TYPE_FRAME_END_INFO};
    end_info.displayTime = p.predicted_display_time;
    end_info.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    end_info.layerCount = layer_count;
    end_info.layers = layer_count ? layers : nullptr;
    const XrResult result = p.xrEndFrame(p.session, &end_info);
    p.frame_begun = false;
    if (XR_FAILED(result)) {
        return p.fail("xrEndFrame failed");
    }
    return true;
}

PassthroughState OpenXrRuntime::passthrough_state() const noexcept {
    return impl_->passthrough;
}

const char* OpenXrRuntime::passthrough_extension() const noexcept {
#ifdef XR_FB_passthrough
    if (impl_->passthrough_layer) return "XR_FB_passthrough";
#endif
#ifdef XR_HTC_passthrough
    if (impl_->htc_passthrough_handle) return "XR_HTC_passthrough";
#endif
#ifdef XR_FB_passthrough
    if (impl_->fb_passthrough_detected) return "XR_FB_passthrough";
#endif
#ifdef XR_HTC_passthrough
    if (impl_->htc_passthrough_detected) return "XR_HTC_passthrough";
#endif
    return "";
}

bool OpenXrRuntime::set_passthrough_enabled(bool enabled) {
    return impl_->set_passthrough(enabled);
}

void OpenXrRuntime::shutdown() {
    if (!impl_) {
        return;
    }
    auto& p = *impl_;
    if (p.frame_begun && p.session_running && p.xrEndFrame) {
        XrFrameEndInfo end_info{XR_TYPE_FRAME_END_INFO};
        end_info.displayTime = p.predicted_display_time;
        end_info.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
        p.xrEndFrame(p.session, &end_info);
    }
    p.frame_begun = false;
    if (p.quad_swapchain && p.xrDestroySwapchain) p.xrDestroySwapchain(p.quad_swapchain);
#ifdef XR_FB_passthrough
    if (p.passthrough == PassthroughState::on) p.set_passthrough(false);
    if (p.passthrough_layer && p.xrDestroyPassthroughLayerFB)
        p.xrDestroyPassthroughLayerFB(p.passthrough_layer);
    if (p.passthrough_handle && p.xrDestroyPassthroughFB)
        p.xrDestroyPassthroughFB(p.passthrough_handle);
    p.passthrough_layer = XR_NULL_HANDLE;
    p.passthrough_handle = XR_NULL_HANDLE;
#endif
#ifdef XR_HTC_passthrough
    if (p.htc_passthrough_handle && p.xrDestroyPassthroughHTC)
        p.xrDestroyPassthroughHTC(p.htc_passthrough_handle);
    p.htc_passthrough_handle = XR_NULL_HANDLE;
#endif
    if (p.session_running && p.xrEndSession && p.session) p.xrEndSession(p.session);
    p.session_running = false;
    for (auto& space : p.aim_spaces) {
        if (space && p.xrDestroySpace) p.xrDestroySpace(space);
        space = XR_NULL_HANDLE;
    }
    if (p.local_space && p.xrDestroySpace) p.xrDestroySpace(p.local_space);
    if (p.action_set && p.xrDestroyActionSet) p.xrDestroyActionSet(p.action_set);
    if (p.session && p.xrDestroySession) p.xrDestroySession(p.session);
    if (p.context) p.context->Release();
    if (p.device) p.device->Release();
    if (p.instance && p.xrDestroyInstance) p.xrDestroyInstance(p.instance);
    if (p.loader) FreeLibrary(p.loader);

    p.quad_swapchain = XR_NULL_HANDLE;
    p.local_space = XR_NULL_HANDLE;
    p.action_set = XR_NULL_HANDLE;
    p.session = XR_NULL_HANDLE;
    p.instance = XR_NULL_HANDLE;
    p.context = nullptr;
    p.device = nullptr;
    p.loader = nullptr;
    p.passthrough = PassthroughState::unavailable;
    p.fb_passthrough_detected = false;
    p.fb_passthrough_enabled = false;
    p.htc_passthrough_detected = false;
    p.htc_passthrough_enabled = false;
}

const char* OpenXrRuntime::last_error() const noexcept {
    return impl_->error.c_str();
}

} // namespace area51xr

#endif // _WIN32
