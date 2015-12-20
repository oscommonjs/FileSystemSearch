#include "PrecompiledHeader.h"
#include "CriticalSection.h"
#include "ExplorerWindow.h"
#include "FileSystemBindData.h"
#include "HandleHolder.h"
#include "WindowClassHolder.h"

static CriticalSection s_WindowClassCriticalSection;
static WindowClassHolder s_WindowClass;

void ExplorerWindow::EnsureWindowClassIsCreated()
{
	CriticalSection::Lock lock(s_WindowClassCriticalSection);

	if (s_WindowClass != 0)
		return;

	WNDCLASSEX windowInfo;
	ZeroMemory(&windowInfo, sizeof(WNDCLASSEX));

	windowInfo.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	windowInfo.hInstance = GetModuleHandleW(nullptr);
	windowInfo.hIcon = nullptr;
	windowInfo.hIconSm = windowInfo.hIcon;
	windowInfo.hCursor = LoadCursor(nullptr, IDC_ARROW);
	windowInfo.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	windowInfo.lpszMenuName = nullptr;
	windowInfo.lpszClassName = L"SearchResultsView";
	windowInfo.cbSize = sizeof(WNDCLASSEX);
	windowInfo.lpfnWndProc = DefWindowProcW;

	s_WindowClass = RegisterClassEx(&windowInfo);
	Assert(s_WindowClass != 0);
}

ExplorerWindow::ExplorerWindow(HWND parent, int width, int height) :
	m_Hwnd(nullptr),
	m_Width(width),
	m_Height(height)
{
	auto hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
	Assert(SUCCEEDED(hr));

	EnsureWindowClassIsCreated();

	m_Hwnd = CreateWindowExW(0, s_WindowClass, L"SearchResultsViewWindow", WS_CHILD, 0, 0, m_Width, m_Height, parent, nullptr, GetModuleHandleW(nullptr), nullptr);
	Assert(m_Hwnd != nullptr);
}

ExplorerWindow::~ExplorerWindow()
{
	if (m_ExplorerBrowser != nullptr)
	{
		auto hr = m_ExplorerBrowser->Destroy();
		Assert(SUCCEEDED(hr));
	}

	m_ExplorerBrowser = nullptr;
	m_ResultsFolder = nullptr;
	m_BindCtx = nullptr;
	m_FileSystemBindData = nullptr;

	CoUninitialize();
}

void ExplorerWindow::Initialize()
{
	auto hr = CoCreateInstance(CLSID_ExplorerBrowser, nullptr, CLSCTX_INPROC, IID_IExplorerBrowser, &m_ExplorerBrowser);
	Assert(SUCCEEDED(hr));

	RECT displayRect = { 0, 0, m_Width, m_Height };
	FOLDERSETTINGS folderSettings = { FVM_DETAILS, 0 };
	hr = m_ExplorerBrowser->Initialize(m_Hwnd, &displayRect, &folderSettings);
	Assert(SUCCEEDED(hr));

	hr = m_ExplorerBrowser->FillFromObject(nullptr, EBF_NONE);
	Assert(SUCCEEDED(hr));

	hr = m_ExplorerBrowser->SetOptions(EBO_NOPERSISTVIEWSTATE | EBO_SHOWFRAMES | EBO_NOBORDER | EBO_NAVIGATEONCE);
	Assert(SUCCEEDED(hr));

	WRL::ComPtr<IFolderView> folderView;
	hr = m_ExplorerBrowser->GetCurrentView(__uuidof(IFolderView), &folderView);
	Assert(SUCCEEDED(hr));

	hr = folderView->GetFolder(__uuidof(IResultsFolder), &m_ResultsFolder);
	Assert(SUCCEEDED(hr));

	WRL::ComPtr<IBindCtx> itemBindContext;

	hr = CreateBindCtx(0, &itemBindContext);
	Assert(SUCCEEDED(hr));

	hr = CreateBindCtx(0, &m_BindCtx);
	Assert(SUCCEEDED(hr));

	hr = m_BindCtx->RegisterObjectParam(STR_ITEM_CACHE_CONTEXT, itemBindContext.Get());
	Assert(SUCCEEDED(hr));

	m_FileSystemBindData = WRL::Make<FileSystemBindData>();

	hr = m_BindCtx->RegisterObjectParam(STR_FILE_SYS_BIND_DATA, m_FileSystemBindData.Get());
	Assert(SUCCEEDED(hr));
}

void ExplorerWindow::Destroy()
{
	DestroyWindow(m_Hwnd);
	delete this;
}

void ExplorerWindow::ResizeView(int width, int height)
{
	auto setPositionResult = SetWindowPos(m_Hwnd, HWND_BOTTOM, 0, 0, width, height, 0);
	Assert(setPositionResult != FALSE);

	RECT rect = { 0, 0, width, height };
	auto hr = m_ExplorerBrowser->SetRect(nullptr, rect);
	Assert(SUCCEEDED(hr));
}

void ExplorerWindow::AddItem(const WIN32_FIND_DATAW* findData, const wchar_t* path)
{
	WRL::ComPtr<IShellItem2> shellItem;

	m_FileSystemBindData->SetFindData(findData);

	auto hr = SHCreateItemFromParsingName(path, m_BindCtx.Get(), __uuidof(IShellItem2), &shellItem);
	Assert(SUCCEEDED(hr));

	if (SUCCEEDED(hr))
	{
		hr = m_ResultsFolder->AddItem(shellItem.Get());
		Assert(SUCCEEDED(hr));
	}

	m_FileSystemBindData->SetFindData(nullptr);
}