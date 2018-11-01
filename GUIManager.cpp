#include "GUIManager.h"
#include "ImGUI/imgui.h"
#include "ImGUI/imgui_impl_win32.h"
#include "ImGUI/imgui_impl_dx11.h"
#include "UI/CodeEditorUI.h"
#include "UI/PipelineUI.h"
#include "UI/PropertyUI.h"
#include "UI/PreviewUI.h"
#include "UI/PinnedUI.h"
#include "Objects/Names.h"

#include <Windows.h>
#include <fstream>

namespace ed
{
	GUIManager::GUIManager(ed::InterfaceManager* objects, ml::Window* wnd)
	{
		m_views.push_back(new PipelineUI(this, objects, "Pipeline"));
		m_views.push_back(new PreviewUI(this, objects, "Preview"));
		m_views.push_back(new PropertyUI(this, objects, "Properties"));
		m_views.push_back(new PinnedUI(this, objects, "Pinned"));
		m_views.push_back(new CodeEditorUI(this, objects, "Code"));

		m_applySize = false;

		m_data = objects;
		m_wnd = wnd;

		// Initialize imgui
		ImGui::CreateContext();
		
		ImGuiIO& io = ImGui::GetIO();
		io.IniFilename = IMGUI_INI_FILE;
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NoMouseCursorChange | ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_ViewportsEnable;
		io.ConfigDockingWithShift = false;

		ImGui_ImplWin32_Init(m_wnd->GetWindowHandle());
		ImGui_ImplDX11_Init(m_wnd->GetDevice(), m_wnd->GetDeviceContext());
		
		ImGui::StyleColorsDark();
		ImGuiStyle& style = ImGui::GetStyle();
		style.Colors[ImGuiCol_TitleBgActive].w = 0.2f;
	}
	GUIManager::~GUIManager()
	{
		for (auto view : m_views)
			delete view;

		// release memory
		ImGui_ImplDX11_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
	}

	void GUIManager::OnEvent(const ml::Event& e)
	{
		m_imguiHandleEvent(e);

		if (e.Type == ml::EventType::WindowResize) {
			m_applySize = true;
		}

		for (auto view : m_views)
			view->OnEvent(e);
	}
	void GUIManager::Update(float delta)
	{
		// Start the Dear ImGui frame
		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		// window flags
		const ImGuiWindowFlags window_flags = (ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

		ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->Pos);
		ImGui::SetNextWindowSize(viewport->Size);
		ImGui::SetNextWindowViewport(viewport->ID);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

		// create window
		ImGui::Begin("DockSpace Demo", nullptr, window_flags);
		ImGui::PopStyleVar(3);


		// dockspace
		ImGuiID dockspace_id = ImGui::GetID("MyDockspace");
		ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruDockspace | ImGuiDockNodeFlags_None);

		// menu
		if (ImGui::BeginMainMenuBar()) {
			if (ImGui::BeginMenu("File")) {
				if (ImGui::MenuItem("New")) {
					m_data->Renderer.FlushCache();
					m_data->Pipeline.New();
				}
				if (ImGui::MenuItem("Open")) {
					m_data->Renderer.FlushCache();
					m_openProject();
				}
				if (ImGui::MenuItem("Save")) {
					if (m_data->Parser.GetOpenedFile() == "")
						m_saveAsProject();
					else
						m_data->Parser.Save();
				}
				if (ImGui::MenuItem("Save As"))
					m_saveAsProject();
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Create")) {
				ImGui::MenuItem("Shader");
				ImGui::MenuItem("Geometry");
				ImGui::MenuItem("Input Layout");
				ImGui::MenuItem("Topology");
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Window")) {
				for (auto view : m_views) {
					if (view->Name != "Code") // dont show the "Code" UI view in this menu
						ImGui::MenuItem(view->Name.c_str(), 0, &view->Visible);
				}
				ImGui::EndMenu();
			}
			ImGui::EndMainMenuBar();
		}


		for (auto view : m_views)
			if (view->Visible) {
				ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
				ImGui::SetNextWindowSizeConstraints(ImVec2(50, 50), ImVec2(m_wnd->GetSize().x, m_wnd->GetSize().y));
				if (ImGui::Begin(view->Name.c_str(), &view->Visible)) view->Update(delta);
				ImGui::End();
			}

		Get("Code")->Update(delta);

		ImGui::End();


		// render ImGUI
		ImGui::Render();
	}
	void GUIManager::Render()
	{
		// actually render to back buffer
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

		// Update and Render additional Platform Windows
		if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
		}
	}
	UIView * GUIManager::Get(const std::string & name)
	{
		for (auto view : m_views)
			if (view->Name == name)
				return view;
		return nullptr;
	}
	void GUIManager::SaveSettings()
	{
		std::ofstream data("gui.dat");

		for (auto view : m_views)
			data.put(view->Visible);

		data.close();
	}
	void GUIManager::LoadSettings()
	{
		std::ifstream data("gui.dat");

		if (data.is_open()) {
			for (auto view : m_views)
				view->Visible = data.get();

			data.close();
		}

		Get("Code")->Visible = false;
	}
	void GUIManager::m_imguiHandleEvent(const ml::Event & e)
	{
		if (ImGui::GetCurrentContext() == NULL)
			return;

		ImGuiIO& io = ImGui::GetIO();
		switch (e.Type) {
			case ml::EventType::MouseButtonPress:
			{
				int button = 0;
				if (e.MouseButton.VK == VK_LBUTTON) button = 0;
				if (e.MouseButton.VK == VK_RBUTTON) button = 1;
				if (e.MouseButton.VK == VK_MBUTTON) button = 2;
				if (!ImGui::IsAnyMouseDown() && GetCapture() == NULL)
					SetCapture(m_wnd->GetWindowHandle());
				io.MouseDown[button] = true;
			}
			break;

			case ml::EventType::MouseButtonRelease:
			{
				int button = 0;
				if (e.MouseButton.VK == VK_LBUTTON) button = 0;
				if (e.MouseButton.VK == VK_RBUTTON) button = 1;
				if (e.MouseButton.VK == VK_MBUTTON) button = 2;
				io.MouseDown[button] = false;
				if (!ImGui::IsAnyMouseDown() && GetCapture() == m_wnd->GetWindowHandle())
					ReleaseCapture();
			}
			break;

			case ml::EventType::Scroll:
				io.MouseWheel += e.MouseWheel.Delta;
			break;

			case ml::EventType::KeyPress:
				io.KeysDown[e.Keyboard.VK] = 1;
			break;

			case ml::EventType::KeyRelease:
				io.KeysDown[e.Keyboard.VK] = 0;
			break;

			case ml::EventType::TextEnter:
				io.AddInputCharacter(e.TextCode);
			break;
		}
	}
	void GUIManager::m_openProject()
	{
		OPENFILENAME dialog;
		TCHAR filePath[MAX_PATH] = { 0 };

		ZeroMemory(&dialog, sizeof(dialog));
		dialog.lStructSize = sizeof(dialog);
		dialog.hwndOwner = m_data->GetOwner()->GetWindowHandle();
		dialog.lpstrFile = filePath;
		dialog.nMaxFile = sizeof(filePath);
		dialog.lpstrFilter = L"HLSLed Project\0*.sprj\0";
		dialog.nFilterIndex = 0;
		dialog.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
		
		if (GetOpenFileName(&dialog) == TRUE) {
			std::wstring wfile = std::wstring(filePath);
			std::string file(wfile.begin(), wfile.end());

			m_data->Parser.Open(file);
		}
	}
	void GUIManager::m_saveAsProject()
	{
		OPENFILENAME dialog;
		TCHAR filePath[MAX_PATH] = { 0 };

		ZeroMemory(&dialog, sizeof(dialog));
		dialog.lStructSize = sizeof(dialog);
		dialog.hwndOwner = m_data->GetOwner()->GetWindowHandle();
		dialog.lpstrFile = filePath;
		dialog.nMaxFile = sizeof(filePath);
		dialog.lpstrFilter = L"HLSLed Project\0*.sprj\0";
		dialog.nFilterIndex = 1;
		dialog.lpstrDefExt = L".sprj";
		dialog.Flags = OFN_PATHMUSTEXIST | OFN_CREATEPROMPT | OFN_OVERWRITEPROMPT;

		if (GetSaveFileName(&dialog) == TRUE) {
			std::wstring wfile = std::wstring(filePath);
			std::string file(wfile.begin(), wfile.end());

			m_data->Parser.SaveAs(file);
		}
	}
}