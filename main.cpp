#include <Windows.h>
#include <cstdint>
#include <string>
#include <format>

#include <d3d12.h>
#include <dxgi1_6.h>
#include <cassert>

#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")

#pragma region ConvertString

std::wstring ConvertString(const std::string& str) {
	if (str.empty()) {
		return std::wstring();
	}

	auto sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(&str[0]), static_cast<int>(str.size()), NULL, 0);
	if (sizeNeeded == 0) {
		return std::wstring();
	}
	std::wstring result(sizeNeeded, 0);
	MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(&str[0]), static_cast<int>(str.size()), &result[0], sizeNeeded);
	return result;
}

std::string ConvertString(const std::wstring& str) {
	if (str.empty()) {
		return std::string();
	}

	auto sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), NULL, 0, NULL, NULL);
	if (sizeNeeded == 0) {
		return std::string();
	}
	std::string result(sizeNeeded, 0);
	WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), result.data(), sizeNeeded, NULL, NULL);
	return result;
}

#pragma endregion

// デバッグ用ログの出力用関数
void Log(const std::string& message)
{
	OutputDebugStringA(message.c_str());
}

// ウィンドウプロシージャ
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	// メッセージに応じたゲーム固有の処理を行う
	switch (msg)
	{
		// ウィンドウが破棄された
	case WM_DESTROY:

		// OSに対いて、アプリの終了を伝える
		PostQuitMessage(0);
		return 0;
	}

	// 標準のメッセージ処理を行う
	return DefWindowProc(hwnd, msg, wparam, lparam);
}

//Windowsアプリでのエントリーポイント（main関数）
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
#pragma region Windowの生成

	WNDCLASS wc{};
	wc.lpfnWndProc = WindowProc;                  // ウィンドウプロシージャ
	wc.lpszClassName = L"CG2WindowClass";         // ウィンドウクラス名（なんでもいい）
	wc.hInstance = GetModuleHandle(nullptr);      // インスタンスハンドル
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);  // カーソル

	// ウィンドウクラスを登録する
	RegisterClass(&wc);

	// クライアント領域のサイズ
	const int32_t kClientWidth = 1280;
	const int32_t kClientHeight = 720;

	// ウィンドウサイズを表す構造体にクライアント領域を入れる
	RECT wrc = { 0,0,kClientWidth,kClientHeight };

	// クライアント領域をもとに実際のサイズにwrcを変更してもらう
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

	// ウィンドウの生成
	HWND hwnd = CreateWindow(
		wc.lpszClassName,      // 利用するクラス名
		L"CG2",                // タイトルバーの文字
		WS_OVERLAPPEDWINDOW,   // よく見るウィンドウスタイル
		CW_USEDEFAULT,         // 表示X座標（Windowに任せる）
		CW_USEDEFAULT,         // 表示Y座標（WindowOsに任せる）
		wrc.right - wrc.left,  // ウィンドウ横幅
		wrc.bottom - wrc.top,  // ウィンドウ縦幅
		nullptr,               // 親ウィンドウハンドル
		nullptr,               // メニューハンドル
		wc.hInstance,          // インスタンスハンドル
		nullptr				   // オプション
	);

	// ウィンドウを表示する
	ShowWindow(hwnd, SW_SHOW);

#pragma endregion

#pragma region DXGIファクトリーの生成

	// DXGIファクトリーの生成
	IDXGIFactory7* dxgifactory = nullptr;

	// HRESULTはWindows系のエラーコードであり、関数が成功したかどうかをSUCCEEDEDマクロで判定できる
	HRESULT hr = CreateDXGIFactory(IID_PPV_ARGS(&dxgifactory));

	// 初期化の根本的な部分でエラーが出た場合はプログラムが間違っているか、
	// どうにもならない場合が多いのでassertにしておく
	assert(SUCCEEDED(hr));

#pragma endregion

#pragma region 使用するアダプター（GPU)を決定する

	// 使用するアダプタ用の変数。最初にnullptrを入れておく
	IDXGIAdapter4* useAdapter = nullptr;

	// 良い順にアダプタを頼む
	for (UINT i = 0; dxgifactory->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
		IID_PPV_ARGS(&useAdapter)) != DXGI_ERROR_NOT_FOUND; ++i)
	{
		// アダプターの情報を取得する
		DXGI_ADAPTER_DESC3 adapterDese{};
		hr = useAdapter->GetDesc3(&adapterDese);
		assert(SUCCEEDED(hr));  // 取得できないのは一大事

		// ソフトウェアアダプタでなければ採用
		if (!(adapterDese.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE))
		{
			// 採用したアダプタの情報をログに出力。wstringのほうなので注意
			Log(ConvertString(std::format(L"Use Adapter:{}\n", adapterDese.Description)));
			break;
		}
		useAdapter = nullptr;  // ソフトウェアアダプタの場合は見なかったことにする
	}
	// 適切なアダプタが見つからなかったので起動できない
	assert(useAdapter != nullptr);

#pragma endregion

#pragma region D3D12Deviceの生成

	ID3D12Device* device = nullptr;

	// 機能レベルとログ出力用の文字列
	D3D_FEATURE_LEVEL featureLevels[] =
	{
		D3D_FEATURE_LEVEL_12_2,D3D_FEATURE_LEVEL_12_1,D3D_FEATURE_LEVEL_12_0
	};

	const char* featureLevelStrings[] = { "12.2","12.1","12.0" };

	// 高い順に生成できるか試していく
	for (size_t i = 0; i < _countof(featureLevels); ++i)
	{
		// 採用したアダプターでデバイスを生成
		hr = D3D12CreateDevice(useAdapter, featureLevels[i], IID_PPV_ARGS(&device));

		// 指定した機能レベルでデバイス生成できたか確認
		if (SUCCEEDED(hr))
		{
			// 生成できたのでログ出力を行ってループを抜ける
			Log(std::format("FeatureLevel : {}\n", featureLevelStrings[i]));
			break;
		}
	}
	// デバイスの生成がうまくいかなかったので起動できない
	assert(device != nullptr);

	// 初期化完了のログを出す
	Log("Complete create D3D12Device!!!\n"); 

#pragma endregion

#pragma region CommandQueueの生成

	// コマンドキューを生成する
	ID3D12CommandQueue* commandQueue = nullptr;
	D3D12_COMMAND_QUEUE_DESC commandQueueDese{};
	hr = device->CreateCommandQueue(&commandQueueDese, IID_PPV_ARGS(&commandQueue));

	// コマンドキューの生成がうまくいかなかったので起動できない
	assert(SUCCEEDED(hr));

#pragma endregion

#pragma region CommandAllocatorの生成

	// コマンドアロケータを生成する
	ID3D12CommandAllocator* commandAllocater = nullptr;
	hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocater));

	// コマンドアロケータの生成がうまくいかなかったので起動できない
	assert(SUCCEEDED(hr));

#pragma endregion

#pragma region CommandLisの生成

	// コマンドリストを生成する
	ID3D12GraphicsCommandList* commandList = nullptr;
	hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocater, nullptr, IID_PPV_ARGS(&commandList));

	// コマンドリストの生成がうまくいかなかったので起動できない
	assert(SUCCEEDED(hr));

#pragma endregion

#pragma region SwapChainの生成

	// スワップチェーンを生成する
	IDXGISwapChain4* swapChain = nullptr;
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
	swapChainDesc.Width = kClientWidth;  // 画面の幅。ウィンドウのクライアント幅を同じものにしておく
	swapChainDesc.Height = kClientHeight;  // 画面を高さ。ウィンドウのクライアント領域を同じものにしておく
	swapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;  // 色の形式
	swapChainDesc.SampleDesc.Count = 1;  // マルチサンプルしない
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;  // 描画のターゲットとして利用する
	swapChainDesc.BufferCount = 2;  // ダブルバッファ
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;  // モニターの写したら、中身を破棄

	// コマンドキュー、ウィンドウハンドル、設定を渡して生成する
	hr = dxgifactory->CreateSwapChainForHwnd(commandQueue, hwnd, &swapChainDesc, nullptr, nullptr, reinterpret_cast<IDXGISwapChain1**>(&swapChain));
	assert(SUCCEEDED(hr));

#pragma endregion

#pragma region DescriptorHeapの生成

	// ディスクリプタヒープの生成
	ID3D12DescriptorHeap* rtvDescriptorHeap = nullptr;
	D3D12_DESCRIPTOR_HEAP_DESC rtvDescriptorHeapDesc{};
	rtvDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvDescriptorHeapDesc.NumDescriptors = 2;
	hr = device->CreateDescriptorHeap(&rtvDescriptorHeapDesc, IID_PPV_ARGS(&rtvDescriptorHeap));

	// ディスクリプタヒープが作れなかったので起動できない
	assert(SUCCEEDED(hr));

#pragma endregion

#pragma region SwapChainからResouceを引っ張ってくる

	// SwapChainからResouceを引っ張ってくる
	ID3D12Resource* swapChainResources[2] = { nullptr };
	hr = swapChain->GetBuffer(0, IID_PPV_ARGS(&swapChainResources[0]));

	// うまく取得できなければ起動できない
	assert(SUCCEEDED(hr));
	hr = swapChain->GetBuffer(1, IID_PPV_ARGS(&swapChainResources[1]));
	assert(SUCCEEDED(hr));

#pragma endregion

#pragma region RTVの生成

	// RTVの設定
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
	rtvDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

	// ディスクリプタの戦闘を取得する
	D3D12_CPU_DESCRIPTOR_HANDLE rtvStartHandle = rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();

	// RTVを２つ作るのでディスクリプタを２つ用意する
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[2];

	// まず１つ目を作る。１つ目は最初のところに作る。作る場所をこちらで指定してあげる必要がある
	rtvHandles[0] = rtvStartHandle;
	device->CreateRenderTargetView(swapChainResources[0], &rtvDesc, rtvHandles[0]);

	// ２つ目のディスクリプタハンドルを得る
	rtvHandles[1].ptr = rtvHandles[0].ptr + device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	// ２つ目を作成する
	device->CreateRenderTargetView(swapChainResources[1], &rtvDesc, rtvHandles[1]);
	
#pragma endregion

	typedef struct D3D12_CPU_DESCRIPTOR_HANDLE
	{
		SIZE_T ptr;
	}D3D12_CPU_DESCRIPTOR_HANDLE;

	rtvHandles[0] = rtvStartHandle;

	rtvHandles[1].ptr = rtvHandles[0].ptr + device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);


	

	//出力ウィンドウへの文字出力
	Log("Hello,DirectX!\n");

	MSG msg{};

	// ウィンドウの×ボタンが押されるまでループ
	while (msg.message != WM_QUIT)
	{
		// Windowにメッセージが来てたら最優先で処理する
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			// これから書き込むバックバッファのインデックスを取得
			UINT backBufferIndex = swapChain->GetCurrentBackBufferIndex();

			// 描画先のRTVを設定する
			commandList->OMSetRenderTargets(1, &rtvHandles[backBufferIndex], false, nullptr);

			// 指定した色で画面全体をクリアにする
			float clearColor[] = { 0.1f,0.25f,0.5f,1.0f };  // 青っぽい色。RGBAの順
			commandList->ClearRenderTargetView(rtvHandles[backBufferIndex], clearColor, 0, nullptr);

			// コマンドリストの内容を確定させる。全てのコマンドを積んでからCloseすること
			hr = commandList->Close();
			assert(SUCCEEDED(hr));

			// GPUにコマンドリストの実行を行わせる
			ID3D12CommandList* commandLists[] = { commandList };
			commandQueue->ExecuteCommandLists(1, commandLists);

			// GPUとOSに画面の交換をおっこなうように通知する
			swapChain->Present(1, 0);

			// 次のフレーム用のコマンドリストを準備
			hr = commandAllocater->Reset();
			assert(SUCCEEDED(hr));
			hr = commandList->Reset(commandAllocater, nullptr);
			assert(SUCCEEDED(hr));
		}
	}
	return 0;
}
