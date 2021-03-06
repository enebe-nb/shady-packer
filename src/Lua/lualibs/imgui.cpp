#include "../lualibs.hpp"
#include "../logger.hpp"
#include <lua.hpp>
#include <LuaBridge/LuaBridge.h>
#define IMGUI_DISABLE_OBSOLETE_FUNCTIONS
#include <imgui.h>
#include <imgui_stdlib.h>

using namespace luabridge;

static inline ImVec2 imgui_ImVec2_create(float x, float y){
    return ImVec2(x, y);
}

static inline ImVec4 imgui_ImVec4_create(float x, float y, float z, float w){
    return ImVec4(x, y, z, w);
}

static inline ImU32 imgui_ImVec4_rgba(ImVec4* v) {
    return ImGui::ColorConvertFloat4ToU32(*v);
}

static inline ImU32 imgui_ImVec4_hsva(ImVec4* v) {
    float r, g, b;
    ImGui::ColorConvertHSVtoRGB(v->x, v->y, v->z, r, g, b);
    return IM_COL32(r, g, b, v->w);
}

static inline void imgui_Text(const char* s){ImGui::Text(s);}
static inline void imgui_TextColored(const ImVec4& c, const char* s){ImGui::TextColored(c, s);}
static inline void imgui_TextDisabled(const char* s){ImGui::TextDisabled(s);}
static inline void imgui_TextWrapped(const char* s){ImGui::TextWrapped(s);}
static inline void imgui_LabelText(const char* l, const char* s){ImGui::LabelText(l, s);}
static inline void imgui_BulletText(const char* s){ImGui::BulletText(s);}
static inline void imgui_SetTooltip(const char* s){ImGui::SetTooltip(s);}

#define Arg(type, name) type name = Stack<type>::get(L, i++);
#define ArgCond(type, name, default, cond) type name = default; if (cond) name = Stack<type>::get(L, i++);
#define ArgOpt(type, name, default) ArgCond(type, name, default, top >= i)
#define ArgPtr(type, name) type* name = 0; if (top >= i) name = new type(Stack<type>::get(L, i++));
#define Ret(type, name) Stack<type>::push(L, name);
#define RetPtr(type, name) if(name) {Stack<type>::push(L, *name); delete name;}
#define FnBegin(name) static inline int impl_##name(lua_State* L) { int i = 0; int top = lua_gettop(L);
#define FnEnd(ret) return ret; }

FnBegin(Begin) Arg(const char*, name) ArgPtr(bool, open) ArgOpt(ImGuiWindowFlags, flags, 0)
    bool ret = ImGui::Begin(name, open, flags);
Ret(bool, ret) RetPtr(bool, open) FnEnd(2)

FnBegin(BeginChild) Arg(const char*, name) ArgOpt(ImVec2, size, ImVec2(0,0)) ArgOpt(bool, border, false) ArgOpt(ImGuiWindowFlags, flags, 0)
    bool ret = ImGui::BeginChild(name, size, border, flags);
Ret(bool, ret) FnEnd(1)

FnBegin(SetWindowPos) ArgCond(const char*, name, NULL, lua_isstring(L, 1)) Arg(ImVec2, pos) ArgOpt(ImGuiCond, cond, 0)
    if (name) ImGui::SetWindowPos(name, pos, cond);
    else ImGui::SetWindowPos(pos, cond);
FnEnd(0)

FnBegin(SetWindowSize) ArgCond(const char*, name, NULL, lua_isstring(L, 1)) Arg(ImVec2, size) ArgOpt(ImGuiCond, cond, 0)
    if (name) ImGui::SetWindowSize(name, size, cond);
    else ImGui::SetWindowSize(size, cond);
FnEnd(0)

FnBegin(SetWindowCollapsed) ArgCond(const char*, name, NULL, lua_isstring(L, 1)) Arg(bool, collapsed) ArgOpt(ImGuiCond, cond, 0)
    if (name) ImGui::SetWindowCollapsed(name, collapsed, cond);
    else ImGui::SetWindowCollapsed(collapsed, cond);
FnEnd(0)

FnBegin(SetWindowFocus) ArgCond(const char*, name, NULL, lua_isstring(L, 1))
    if (name) ImGui::SetWindowFocus(name);
    else ImGui::SetWindowFocus();
FnEnd(0)

FnBegin(PushStyleColor) Arg(int, idx) ArgCond(int, valI, 0, lua_isnumber(L, 2)) ArgCond(ImVec4, valV, ImVec4(), !lua_isnumber(L, 2))
    if (lua_isnumber(L, 2)) ImGui::PushStyleColor(idx, valI);
    else ImGui::PushStyleColor(idx, valV);
FnEnd(0)

FnBegin(PushStyleVar) Arg(int, idx) ArgCond(float, valF, 0, lua_isnumber(L, 2)) ArgCond(ImVec2, valV, ImVec2(), !lua_isnumber(L, 2))
    if (lua_isnumber(L, 2)) ImGui::PushStyleVar(idx, valF);
    else ImGui::PushStyleVar(idx, valV);
FnEnd(0)

FnBegin(Checkbox) Arg(const char*, label) ArgPtr(bool, v)
    bool ret = ImGui::Checkbox(label, v);
Ret(bool, ret) RetPtr(bool, v) FnEnd(2)

FnBegin(CheckboxFlags) Arg(const char*, label) ArgPtr(unsigned int, flags) Arg(unsigned int, flags_value)
    Logger::Warning("imgui.CheckboxFlags may not well because the way lua handle integers");
    bool ret = ImGui::CheckboxFlags(label, flags, flags_value);
Ret(bool, ret) RetPtr(unsigned int, flags) FnEnd(2)

FnBegin(InputText) Arg(const char*, label) Arg(std::string, buffer) ArgOpt(int, flags, 0) // TODO callback
    bool ret = ImGui::InputText(label, &buffer, flags);
Ret(bool, ret) Ret(std::string, buffer) FnEnd(2)

FnBegin(InputTextMultiline) Arg(const char*, label) Arg(std::string, buffer) ArgOpt(ImVec2, size, ImVec2(0, 0)) ArgOpt(int, flags, 0) // TODO callback
    bool ret = ImGui::InputTextMultiline(label, &buffer, size, flags);
Ret(bool, ret) Ret(std::string, buffer) FnEnd(2)

FnBegin(InputTextWithHint) Arg(const char*, label) Arg(const char*, hint) Arg(std::string, buffer) ArgOpt(int, flags, 0) // TODO callback
    bool ret = ImGui::InputTextWithHint(label, hint, &buffer, flags);
Ret(bool, ret) Ret(std::string, buffer) FnEnd(2)

FnBegin(CalcTextSize) Arg(std::string, text) ArgOpt(bool, hide_text, false) ArgOpt(float, wrap_width, -1.f)
    ImVec2 size = ImGui::CalcTextSize(text.data(), text.data()+text.size(), hide_text, wrap_width);
Ret(ImVec2, size) FnEnd(1)

void ShadyLua::LualibImGui(lua_State* L) {
    getGlobalNamespace(L)
        .beginNamespace("imgui")
            .beginClass<ImVec2>("ImVec2")
                .addConstructor<void(*)()>()
                .addStaticFunction("create", imgui_ImVec2_create)
                .addData("x", &ImVec2::x)
                .addData("y", &ImVec2::y)
            .endClass()
            .beginClass<ImVec4>("ImVec4")
                .addConstructor<void(*)()>()
                .addStaticFunction("create", imgui_ImVec4_create)
                .addData("x", &ImVec4::x).addData("r", &ImVec4::x).addData("h", &ImVec4::x)
                .addData("y", &ImVec4::y).addData("g", &ImVec4::y).addData("s", &ImVec4::y)
                .addData("z", &ImVec4::z).addData("b", &ImVec4::z).addData("v", &ImVec4::z)
                .addData("w", &ImVec4::w).addData("a", &ImVec4::w)
                //.addFunction("rgba", imgui_ImVec4_rgba)
                //.addFunction("hsva", imgui_ImVec4_hsva)
            .endClass()

            // Windows
            .addCFunction("Begin", impl_Begin)
            .addFunction("End", ImGui::End)
            .addCFunction("BeginChild", impl_BeginChild)
            .addFunction("EndChild", ImGui::EndChild)

            // Windows Utilities
            .addFunction("IsWindowAppearing", ImGui::IsWindowAppearing)
            .addFunction("IsWindowCollapsed", ImGui::IsWindowCollapsed)
            .addFunction("IsWindowFocused", ImGui::IsWindowFocused)
            .addFunction("IsWindowHovered", ImGui::IsWindowHovered)
            .addFunction("GetWindowDrawList", ImGui::GetWindowDrawList)
            .addFunction("GetWindowPos", ImGui::GetWindowPos)
            .addFunction("GetWindowSize", ImGui::GetWindowSize)
            .addFunction("GetWindowWidth", ImGui::GetWindowWidth)
            .addFunction("GetWindowHeight", ImGui::GetWindowHeight)
            
            .addFunction("SetNextWindowPos", ImGui::SetNextWindowPos)
            .addFunction("SetNextWindowSize", ImGui::SetNextWindowSize)
            //.addFunction("SetNextWindowSizeConstraints", ImGui::SetNextWindowSizeConstraints)
            .addFunction("SetNextWindowContentSize", ImGui::SetNextWindowContentSize)
            .addFunction("SetNextWindowCollapsed", ImGui::SetNextWindowCollapsed)
            .addFunction("SetNextWindowFocus", ImGui::SetNextWindowFocus)
            .addFunction("SetNextWindowBgAlpha", ImGui::SetNextWindowBgAlpha)
            .addCFunction("SetWindowPos", impl_SetWindowPos)
            .addCFunction("SetWindowSize", impl_SetWindowSize)
            .addCFunction("SetWindowCollapsed", impl_SetWindowCollapsed)
            .addCFunction("SetWindowFocus", impl_SetWindowFocus)
            .addFunction("SetWindowFontScale", ImGui::SetWindowFontScale)

            // Content region
            .addFunction("GetContentRegionMax", ImGui::GetContentRegionMax)
            .addFunction("GetContentRegionAvail", ImGui::GetContentRegionAvail)
            .addFunction("GetWindowContentRegionMin", ImGui::GetWindowContentRegionMin)
            .addFunction("GetWindowContentRegionMax", ImGui::GetWindowContentRegionMax)
            .addFunction("GetWindowContentRegionWidth", ImGui::GetWindowContentRegionWidth)

            // Windows Scrolling
            .addFunction("GetScrollX", ImGui::GetScrollX)
            .addFunction("GetScrollY", ImGui::GetScrollY)
            .addFunction("GetScrollMaxX", ImGui::GetScrollMaxX)
            .addFunction("GetScrollMaxY", ImGui::GetScrollMaxY)
            .addFunction("SetScrollX", ImGui::SetScrollX)
            .addFunction("SetScrollY", ImGui::SetScrollY)
            .addFunction("SetScrollHereX", ImGui::SetScrollHereX)
            .addFunction("SetScrollHereY", ImGui::SetScrollHereY)
            .addFunction("SetScrollFromPosX", ImGui::SetScrollFromPosX)
            .addFunction("SetScrollFromPosY", ImGui::SetScrollFromPosY)

            // Parameters stacks (shared)
            //.addFunction("PushFont", ImGui::PushFont)
            //.addFunction("PopFont", ImGui::PopFont)
            .addCFunction("PushStyleColor", impl_PushStyleColor)
            .addFunction("PopStyleColor", ImGui::PopStyleColor)
            .addCFunction("PushStyleVar", impl_PushStyleVar)
            .addFunction("PopStyleVar", ImGui::PopStyleVar)
            .addFunction("PushAllowKeyboardFocus", ImGui::PushAllowKeyboardFocus)
            .addFunction("PopAllowKeyboardFocus", ImGui::PopAllowKeyboardFocus)
            .addFunction("PushButtonRepeat", ImGui::PushButtonRepeat)
            .addFunction("PopButtonRepeat", ImGui::PopButtonRepeat)

            // Parameters stacks (current window)
            .addFunction("PushItemWidth", ImGui::PushItemWidth)
            .addFunction("PopItemWidth", ImGui::PopItemWidth)
            .addFunction("SetNextItemWidth", ImGui::SetNextItemWidth)
            .addFunction("CalcItemWidth", ImGui::CalcItemWidth)
            .addFunction("PushTextWrapPos", ImGui::PushTextWrapPos)
            .addFunction("PopTextWrapPos", ImGui::PopTextWrapPos)

            //.addFunction("GetFont", ImGui::GetFont)
            .addFunction("GetFontSize", ImGui::GetFontSize)
            .addFunction("GetFontTexUvWhitePixel", ImGui::GetFontTexUvWhitePixel)
            //.addFunction("GetColor", ImGui::GetColor))
            .addFunction("GetStyleColor", ImGui::GetStyleColorVec4)

            // Cursor / Layout
            .addFunction("Separator", ImGui::Separator)
            .addFunction("SameLine", ImGui::SameLine)
            .addFunction("NewLine", ImGui::NewLine)
            .addFunction("Spacing", ImGui::Spacing)
            .addFunction("Dummy", ImGui::Dummy)
            .addFunction("Indent", ImGui::Indent)
            .addFunction("Unindent", ImGui::Unindent)
            .addFunction("BeginGroup", ImGui::BeginGroup)
            .addFunction("EndGroup", ImGui::EndGroup)

            .addFunction("GetCursorPos", ImGui::GetCursorPos)
            .addFunction("GetCursorPosX", ImGui::GetCursorPosX)
            .addFunction("GetCursorPosY", ImGui::GetCursorPosY)
            .addFunction("SetCursorPos", ImGui::SetCursorPos)
            .addFunction("SetCursorPosX", ImGui::SetCursorPosX)
            .addFunction("SetCursorPosY", ImGui::SetCursorPosY)
            .addFunction("GetCursorStartPos", ImGui::GetCursorStartPos)
            .addFunction("GetCursorScreenPos", ImGui::GetCursorScreenPos)
            .addFunction("SetCursorScreenPos", ImGui::SetCursorScreenPos)

            .addFunction("AlignTextToFramePadding", ImGui::AlignTextToFramePadding)
            .addFunction("GetTextLineHeight", ImGui::GetTextLineHeight)
            .addFunction("GetTextLineHeightWithSpacing", ImGui::GetTextLineHeightWithSpacing)
            .addFunction("GetFrameHeight", ImGui::GetFrameHeight)
            .addFunction("GetFrameHeightWithSpacing", ImGui::GetFrameHeightWithSpacing)

            // ID stack/scopes
            // TODO implement variable arguments
            .addFunction("PushID", static_cast<void(*)(const char*)>(ImGui::PushID))
            .addFunction("PopID", ImGui::PopID)
            .addFunction("GetID", static_cast<ImGuiID(*)(const char*)>(ImGui::GetID))

            // Widgets: Text
            .addFunction("Text", imgui_Text)
            .addFunction("TextColored", imgui_TextColored)
            .addFunction("TextDisabled", imgui_TextDisabled)
            .addFunction("TextWrapped", imgui_TextWrapped)
            .addFunction("LabelText", imgui_LabelText)
            .addFunction("BulletText", imgui_BulletText)

            // Widgets: Main
            .addFunction("Button", ImGui::Button)
            .addFunction("SmallButton", ImGui::SmallButton)
            .addFunction("InvisibleButton", ImGui::InvisibleButton)
            .addFunction("ArrowButton", ImGui::ArrowButton)
            //.addFunction("Image", ImGui::Image)
            //.addFunction("ImageButton", ImGui::ImageButton)
            .addFunction("Checkbox", impl_Checkbox)
            //.addFunction("CheckboxFlags", impl_CheckboxFlags)
            .addFunction("RadioButton", static_cast<bool(*)(const char*, bool)>(ImGui::RadioButton))
            .addFunction("ProgressBar", ImGui::ProgressBar)
            .addFunction("Bullet", ImGui::Bullet)

            // Widgets: Combo Box
            // TODO analyze this to know how to work better in lua
            //.addFunction("BeginCombo", ImGui::BeginCombo)
            //.addFunction("EndCombo", ImGui::EndCombo)
            //.addFunction("Combo", ImGui::Combo)

            // Widgets: Drags
            //.addFunction("DragFloat", ImGui::DragFloat)
            //.addFunction("DragFloatRange2", ImGui::DragFloatRange2)
            //.addFunction("DragInt", ImGui::DragInt)
            //.addFunction("DragIntRange2", ImGui::DragIntRange2)
            //.addFunction("DragScalar", ImGui::DragScalar)
            //.addFunction("DragScalarN", ImGui::DragScalarN)

            // Widgets: Sliders
            //.addFunction("SliderFloat", ImGui::SliderFloat)
            //.addFunction("SliderAngle", ImGui::SliderAngle)
            //.addFunction("SliderInt", ImGui::SliderInt)
            //.addFunction("SliderScalar", ImGui::SliderScalar)
            //.addFunction("SliderScalarN", ImGui::SliderScalarN)
            //.addFunction("VSliderFloat", ImGui::SliderScalar)
            //.addFunction("VSliderInt", ImGui::SliderScalar)
            //.addFunction("VSliderScalar", ImGui::SliderScalar)

            // Widgets: Input with Keyboard
            .addFunction("InputText", impl_InputText)
            .addFunction("InputTextMultiline", impl_InputTextMultiline)
            .addFunction("InputTextWithHint", impl_InputTextWithHint)
            //.addFunction("InputFloat", static_cast<bool(*)(const char*, float*, float, float, const char*, ImGuiInputTextFlags)>(ImGui::InputFloat))
            //.addFunction("InputInt", ImGui::InputInt)
            //.addFunction("InputDouble", ImGui::InputDouble)
            //.addFunction("InputScalar", ImGui::InputScalar)
            //.addFunction("InputScalarN", ImGui::InputScalarN)

            // Widgets: Color Editor/Picker
            //.addFunction("ColorEdit3", ImGui::ColorEdit3)
            //.addFunction("ColorEdit4", ImGui::ColorEdit4)
            //.addFunction("ColorPicker3", ImGui::ColorPicker3)
            //.addFunction("ColorPicker4", ImGui::ColorPicker4)
            .addFunction("ColorButton", ImGui::ColorButton)
            .addFunction("SetColorEditOptions", ImGui::SetColorEditOptions)

            // Widgets: Trees
            // TODO handle variadics
            .addFunction("TreeNode", static_cast<bool(*)(const char*)>(ImGui::TreeNode))
            .addFunction("TreeNodeEx", static_cast<bool(*)(const char*, ImGuiTreeNodeFlags)>(ImGui::TreeNodeEx))
            .addFunction("TreePush", static_cast<void(*)(const char*)>(ImGui::TreePush))
            .addFunction("TreePop", ImGui::TreePop)
            .addFunction("GetTreeNodeToLabelSpacing", ImGui::GetTreeNodeToLabelSpacing)
            // TODO ArgPtr
            .addFunction("CollapsingHeader", static_cast<bool(*)(const char*, ImGuiTreeNodeFlags)>(ImGui::CollapsingHeader))
            .addFunction("SetNextItemOpen", ImGui::SetNextItemOpen)

            // Widgets: Selectables
            // TODO ArgPtr
            .addFunction("Selectable", static_cast<bool(*)(const char*, bool, ImGuiSelectableFlags, const ImVec2&)>(ImGui::Selectable))

            // Widgets: List Boxes
            //.addFunction("ListBox", static_cast<bool(*)(const char*, int*, const char* const[], int, int)>(ImGui::ListBox))
            //.addFunction("ListBoxHeader", static_cast<bool(*)(const char*, const ImVec2&)>(ImGui::ListBoxHeader))
            //.addFunction("ListBoxFooter", ImGui::ListBoxFooter)

            // Widgets: Data Plotting
            //.addFunction("PlotLines", ImGui::PlotLines)
            //.addFunction("PlotHistogram", ImGui::PlotHistogram)

            // Widgets: Value() Helpers.
            .addFunction("Value", static_cast<void(*)(const char*, int)>(ImGui::Value))
            // for format option just use imgui.Text(string.format(...))

            // Widgets: Menus
            .addFunction("BeginMainMenuBar", ImGui::BeginMainMenuBar)
            .addFunction("EndMainMenuBar", ImGui::EndMainMenuBar)
            .addFunction("BeginMenuBar", ImGui::BeginMenuBar)
            .addFunction("EndMenuBar", ImGui::EndMenuBar)
            .addFunction("BeginMenu", ImGui::BeginMenu)
            .addFunction("EndMenu", ImGui::EndMenu)
            // TODO ArgPtr
            .addFunction("MenuItem", static_cast<bool(*)(const char*, const char*, bool, bool)>(ImGui::MenuItem))

            // Tooltips
            .addFunction("BeginTooltip", ImGui::BeginTooltip)
            .addFunction("EndTooltip", ImGui::EndTooltip)
            .addFunction("SetTooltip", imgui_SetTooltip)

            // Popups, Modals
            .addFunction("BeginPopup", ImGui::BeginPopup)
            // TODO ArgPtr
            //.addFunction("BeginPopupModal", ImGui::BeginPopupModal)
            .addFunction("EndPopup", ImGui::EndPopup)

            .addFunction("OpenPopup", ImGui::OpenPopup)
            .addFunction("OpenPopupOnItemClick", ImGui::OpenPopupOnItemClick)
            .addFunction("CloseCurrentPopup", ImGui::CloseCurrentPopup)

            .addFunction("BeginPopupContextItem", ImGui::BeginPopupContextItem)
            .addFunction("BeginPopupContextWindow", ImGui::BeginPopupContextWindow)
            .addFunction("BeginPopupContextVoid", ImGui::BeginPopupContextVoid)
            .addFunction("IsPopupOpen", ImGui::IsPopupOpen)

            // Table TODO (is Beta)

            // Columns
            .addFunction("Columns", ImGui::Columns)
            .addFunction("NextColumn", ImGui::NextColumn)
            .addFunction("GetColumnIndex", ImGui::GetColumnIndex)
            .addFunction("GetColumnWidth", ImGui::GetColumnWidth)
            .addFunction("SetColumnWidth", ImGui::SetColumnWidth)
            .addFunction("GetColumnOffset", ImGui::GetColumnOffset)
            .addFunction("SetColumnOffset", ImGui::SetColumnOffset)
            .addFunction("GetColumnsCount", ImGui::GetColumnsCount)

            // Tab Bars, Tabs
            .addFunction("BeginTabBar", ImGui::BeginTabBar)
            .addFunction("EndTabBar", ImGui::EndTabBar)
            // TODO ArgPtr
            //.addFunction("BeginTabItem", ImGui::BeginTabItem)
            .addFunction("EndTabItem", ImGui::EndTabItem)
            .addFunction("TabItemButton", ImGui::TabItemButton)
            .addFunction("SetTabItemClosed", ImGui::SetTabItemClosed)

            // Drag and Drop TODO

            // Clipping
            .addFunction("PushClipRect", ImGui::PushClipRect)
            .addFunction("PopClipRect", ImGui::PopClipRect)

            // Focus, Activation
            .addFunction("SetItemDefaultFocus", ImGui::SetItemDefaultFocus)
            .addFunction("SetKeyboardFocusHere", ImGui::SetKeyboardFocusHere)

            // Item/Widgets Utilities
            .addFunction("IsItemHovered", ImGui::IsItemHovered)
            .addFunction("IsItemActive", ImGui::IsItemActive)
            .addFunction("IsItemFocused", ImGui::IsItemFocused)
            .addFunction("IsItemClicked", ImGui::IsItemClicked)
            .addFunction("IsItemVisible", ImGui::IsItemVisible)
            .addFunction("IsItemEdited", ImGui::IsItemEdited)
            .addFunction("IsItemActivated", ImGui::IsItemActivated)
            .addFunction("IsItemDeactivated", ImGui::IsItemDeactivated)
            .addFunction("IsItemDeactivatedAfterEdit", ImGui::IsItemDeactivatedAfterEdit)
            .addFunction("IsItemToggledOpen", ImGui::IsItemToggledOpen)
            .addFunction("IsAnyItemHovered", ImGui::IsAnyItemHovered)
            .addFunction("IsAnyItemActive", ImGui::IsAnyItemActive)
            .addFunction("IsAnyItemFocused", ImGui::IsAnyItemFocused)
            .addFunction("GetItemRectMin", ImGui::GetItemRectMin)
            .addFunction("GetItemRectMax", ImGui::GetItemRectMax)
            .addFunction("GetItemRectSize", ImGui::GetItemRectSize)
            .addFunction("SetItemAllowOverlap", ImGui::SetItemAllowOverlap)

            // Miscellaneous Utilities
            .addFunction("IsRectVisible", static_cast<bool(*)(const ImVec2&, const ImVec2&)>(ImGui::IsRectVisible))
            .addFunction("GetTime", ImGui::GetTime)
            .addFunction("GetFrameCount", ImGui::GetFrameCount)
            // drawlists wont work well
            .addFunction("GetStyleColorName", ImGui::GetStyleColorName)
            // state storage also wont
            //.addFunction("CalcListClipping", ImGui::CalcListClipping)
            //.addFunction("CalcListClipping", ImGui::CalcListClipping)
            .addFunction("BeginChildFrame", ImGui::BeginChildFrame)
            .addFunction("EndChildFrame", ImGui::EndChildFrame)
            .addCFunction("CalcTextSize", impl_CalcTextSize)

            // Color Utilities TODO
            .addFunction("ColorConvertU32ToFloat4", ImGui::ColorConvertU32ToFloat4)
            .addFunction("ColorConvertFloat4ToU32", ImGui::ColorConvertFloat4ToU32)
            //.addFunction("ColorConvertRGBtoHSV", ImGui::ColorConvertRGBtoHSV)
            //.addFunction("ColorConvertHSVtoRGB", ImGui::ColorConvertHSVtoRGB)

            // Inputs Utilities
            .addFunction("GetKeyIndex", ImGui::GetKeyIndex)
            .addFunction("IsKeyDown", ImGui::IsKeyDown)
            .addFunction("IsKeyPressed", ImGui::IsKeyPressed)
            .addFunction("IsKeyReleased", ImGui::IsKeyReleased)
            .addFunction("GetKeyPressedAmount", ImGui::GetKeyPressedAmount)
            .addFunction("CaptureKeyboardFromApp", ImGui::CaptureKeyboardFromApp)
            
            .addFunction("IsMouseDown", ImGui::IsMouseDown)
            .addFunction("IsMouseClicked", ImGui::IsMouseClicked)
            .addFunction("IsMouseReleased", ImGui::IsMouseReleased)
            .addFunction("IsMouseDoubleClicked", ImGui::IsMouseDoubleClicked)
            .addFunction("IsMouseHoveringRect", ImGui::IsMouseHoveringRect)
            .addFunction("IsMousePosValid", ImGui::IsMousePosValid)
            .addFunction("IsAnyMouseDown", ImGui::IsAnyMouseDown)
            .addFunction("GetMousePos", ImGui::GetMousePos)
            .addFunction("GetMousePosOnOpeningCurrentPopup", ImGui::GetMousePosOnOpeningCurrentPopup)
            .addFunction("IsMouseDragging", ImGui::IsMouseDragging)
            .addFunction("GetMouseDragDelta", ImGui::GetMouseDragDelta)
            .addFunction("ResetMouseDragDelta", ImGui::ResetMouseDragDelta)
            .addFunction("GetMouseCursor", ImGui::GetMouseCursor)
            .addFunction("SetMouseCursor", ImGui::SetMouseCursor)
            .addFunction("CaptureMouseFromApp", ImGui::CaptureMouseFromApp)

            // Clipboard Utilities (also see the LogToClipboard() function to capture or output text data to the clipboard)
            .addFunction("GetClipboardText", ImGui::GetClipboardText)
            .addFunction("SetClipboardText", ImGui::SetClipboardText)

            // TODO Flags and Constants
        .endNamespace()
    ;
}