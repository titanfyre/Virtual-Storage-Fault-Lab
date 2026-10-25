#include "../SDK/StorageFault.h"
#include <commctrl.h>
#include <shellapi.h>
#include <wincodec.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

namespace {
constexpr COLORREF Background=RGB(14,18,25),Panel=RGB(24,31,42),Ink=RGB(235,241,250),Muted=RGB(151,170,194),Blue=RGB(36,104,245);
enum Id{Enable=1,Clear,Refresh,Setup,First,Count,Every,Read,Write,Copy};
HWND window{},first{},count{},every{},readBox{},writeBox{};
HFONT body{},heading{},number{};HBRUSH background{},panel{};
bool preview=false,closing=false;
std::thread worker;
std::mutex mutex;std::condition_variable changed;
enum class Action{None,Apply,Stop,Refresh,Exit};
Action pending=Action::None;
SF_POLICY requested{};
struct View{SF_CONTROL status{};bool connected=false,owned=false;std::wstring note=L"Reading the virtual adapter...";std::deque<double> history;};
View view;
std::wstring Wide(const std::string& text){int length=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),static_cast<int>(text.size()),nullptr,0);if(!length)return L"The operation failed; the error could not be decoded.";std::wstring out(static_cast<size_t>(length),0);MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),static_cast<int>(text.size()),out.data(),length);return out;}
void Text(HDC dc,int x,int y,int width,const std::wstring& text,HFONT font,COLORREF color=Ink,int flags=DT_LEFT|DT_TOP|DT_NOPREFIX){auto previous=SelectObject(dc,font);SetTextColor(dc,color);SetBkMode(dc,TRANSPARENT);RECT r{x,y,x+width,y+70};DrawTextW(dc,text.c_str(),-1,&r,flags);SelectObject(dc,previous);}
void Fill(HDC dc,RECT r,COLORREF color){HBRUSH brush=CreateSolidBrush(color);FillRect(dc,&r,brush);DeleteObject(brush);}
void Card(HDC dc,RECT r){HBRUSH brush=CreateSolidBrush(Panel);auto oldBrush=SelectObject(dc,brush);auto oldPen=SelectObject(dc,GetStockObject(NULL_PEN));RoundRect(dc,r.left,r.top,r.right,r.bottom,18,18);SelectObject(dc,oldPen);SelectObject(dc,oldBrush);DeleteObject(brush);}
std::wstring Number(SF_U64 value){return std::to_wstring(value);}
View Snapshot(){std::lock_guard<std::mutex> held(mutex);return view;}
void Draw(HDC dc,int width,int height){
 Fill(dc,{0,0,width,height},Background);auto state=Snapshot();
 Text(dc,30,24,width-60,L"Virtual Storage Fault Lab",heading);
 Text(dc,31,63,width-60,preview?L"OFFLINE PREVIEW / explicitly synthetic data":L"Kernel virtual disk / deterministic storage error testing",body,Muted);
 Card(dc,{28,110,width-28,198});
 Text(dc,48,127,width-96,preview?L"PREVIEW":state.connected?(state.status.Stats.PolicyActive?L"FAULT POLICY ACTIVE":L"FAULTS DISABLED"):L"DRIVER UNAVAILABLE",heading,state.status.Stats.PolicyActive?Blue:Ink);
 Text(dc,49,164,width-98,L"64 MiB RAM only  /  512-byte sectors  /  physical disks are never targeted",body,Muted);
 const int right=width-365;
 Card(dc,{28,216,right-18,480});Card(dc,{right,216,width-28,480});
 Text(dc,48,234,right-80,L"Choose a bounded fault range",heading);
 Text(dc,48,276,170,L"First sector",body,Muted);Text(dc,226,276,150,L"Sector count",body,Muted);Text(dc,404,276,140,L"Fail every N",body,Muted);
 Text(dc,48,409,right-80,L"The first 1 MiB is excluded from fault policies.\nThe lab disk is volatile: its data disappears when the adapter is removed or Windows restarts.",body,Muted);
 Text(dc,right+20,234,300,L"Adapter counters",heading);
 const std::array<std::pair<const wchar_t*,SF_U64>,4> metrics{{{L"Read requests",state.status.Stats.Reads},{L"Write requests",state.status.Stats.Writes},{L"Matching requests",state.status.Stats.Matching},{L"Injected errors",state.status.Stats.Injected}}};
 int y=286;for(const auto& metric:metrics){Text(dc,right+20,y,190,metric.first,body,Muted);Text(dc,width-148,y,95,Number(metric.second),number,Ink,DT_RIGHT|DT_TOP);y+=37;}
 Card(dc,{28,498,width-28,height-32});Text(dc,48,516,width-96,L"Session and diagnostics",heading);
 Text(dc,48,555,width-118,state.note,body,state.connected?Ink:Muted);
 std::wstring lease=state.owned?L"Owned session / lease remaining: "+std::to_wstring(state.status.Stats.LeaseRemaining100ns/10000000)+L" s / automatic heartbeat":L"No policy lease owned by this application";
 Text(dc,48,height-76,width-96,preview?L"Synthetic counters; no adapter, disk, policy or file was opened.":lease,body,Muted);
}
void Wake(Action action,const SF_POLICY* policy=nullptr){std::lock_guard<std::mutex> held(mutex);pending=action;if(policy)requested=*policy;changed.notify_one();}
void Poll(){
 StorageFault::Session session;bool owned=false;SF_U64 previous=0;
 for(;;){
  Action action;SF_POLICY policy{};
  {std::unique_lock<std::mutex> held(mutex);changed.wait_for(held,std::chrono::milliseconds(500),[]{return pending!=Action::None;});action=pending;pending=Action::None;policy=requested;}
  if(action==Action::Exit)break;
  try{
   if(action==Action::Apply){if(!owned){session.Start();owned=true;}session.SetPolicy(policy);}
   if(action==Action::Stop){session.Stop();owned=false;}
   auto status=owned?session.Status():StorageFault::Inspect();
   std::lock_guard<std::mutex> held(mutex);view.status=status;view.connected=true;view.owned=owned;
   if(status.Stats.PolicyActive){view.note=L"Every "+Number(status.Policy.Every)+L" matching request(s) fails in sectors "+Number(status.Policy.FirstBlock)+L".."+Number(status.Policy.FirstBlock+status.Policy.BlockCount-1)+L". Clear faults to restore normal lab-disk I/O.";}
   else view.note=L"Normal lab-disk I/O. No fault rule is active. Select a sector range and enable a policy only for a disposable test workload.";
   if(view.history.size()==64)view.history.pop_front();const SF_U64 total=status.Stats.Reads+status.Stats.Writes;view.history.push_back(static_cast<double>(total>=previous?total-previous:0));previous=total;
  }catch(const std::exception& e){
   session.Stop();owned=false;std::lock_guard<std::mutex> held(mutex);view.connected=false;view.owned=false;view.note=Wide(e.what());
  }
  PostMessageW(window,WM_APP+1,0,0);
 }
 session.Stop();
}
SF_U64 ReadNumber(HWND control,SF_U64 maximum){wchar_t text[32]{};int length=GetWindowTextW(control,text,32);if(!length||length>=31)throw std::invalid_argument("Enter a bounded unsigned integer in each field.");SF_U64 value=0;for(int i=0;i<length;++i){unsigned digit=static_cast<unsigned>(text[i]-L'0');if(text[i]<L'0'||text[i]>L'9'||value>(maximum-digit)/10)throw std::invalid_argument("A field contains an invalid or out-of-range integer.");value=value*10+digit;}return value;}
HWND Button(const wchar_t* label,int id,int x,int y,int width){HWND h=CreateWindowW(L"BUTTON",label,WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,x,y,width,38,window,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),nullptr,nullptr);SendMessageW(h,WM_SETFONT,reinterpret_cast<WPARAM>(body),TRUE);return h;}
void CopyReport(){auto state=Snapshot();std::wstring report=L"Virtual Storage Fault Lab\r\n"+state.note+L"\r\nReads: "+Number(state.status.Stats.Reads)+L"\r\nWrites: "+Number(state.status.Stats.Writes)+L"\r\nInjected errors: "+Number(state.status.Stats.Injected)+L"\r\nGeneration: "+Number(state.status.Generation)+L"\r\n";if(!OpenClipboard(window))return;HGLOBAL memory=GlobalAlloc(GMEM_MOVEABLE,(report.size()+1)*sizeof(wchar_t));if(memory){void* data=GlobalLock(memory);if(data){memcpy(data,report.c_str(),(report.size()+1)*sizeof(wchar_t));GlobalUnlock(memory);EmptyClipboard();if(!SetClipboardData(CF_UNICODETEXT,memory))GlobalFree(memory);}else GlobalFree(memory);}CloseClipboard();}
void SetupDriver(){wchar_t file[32768]{};DWORD length=GetModuleFileNameW(nullptr,file,32768);if(!length||length>=32768)return;std::wstring path(file,length);path.resize(path.find_last_of(L"\\/")+1);path+=L"StorageFaultLabSetup.exe";if(GetFileAttributesW(path.c_str())==INVALID_FILE_ATTRIBUTES){MessageBoxW(window,L"The Microsoft-signed setup is not available in this source/preview bundle. Use the installable signed release when it is published. Never disable Windows security to install an unsigned production package.",L"Driver setup",MB_OK|MB_ICONINFORMATION);return;}ShellExecuteW(window,L"runas",path.c_str(),nullptr,nullptr,SW_SHOW);}
LRESULT CALLBACK Procedure(HWND h,UINT message,WPARAM w,LPARAM l){
 switch(message){
 case WM_CREATE:{
  window=h;body=CreateFontW(17,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");heading=CreateFontW(24,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");number=CreateFontW(18,0,0,0,FW_MEDIUM,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Consolas");background=CreateSolidBrush(Background);panel=CreateSolidBrush(Panel);
  Button(L"Driver setup",Setup,833,35,150);Button(L"Enable faults",Enable,48,359,160);Button(L"Clear faults",Clear,222,359,140);Button(L"Refresh",Refresh,376,359,130);Button(L"Copy report",Copy,835,510,145);
  auto edit=[h](int id,int x,const wchar_t* value){HWND field=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",value,WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_NUMBER|ES_AUTOHSCROLL,x,302,150,30,h,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),nullptr,nullptr);SendMessageW(field,EM_SETLIMITTEXT,10,0);SendMessageW(field,WM_SETFONT,reinterpret_cast<WPARAM>(body),TRUE);return field;};
  first=edit(First,48,L"4096");count=edit(Count,226,L"128");every=edit(Every,404,L"4");
  readBox=CreateWindowW(L"BUTTON",L"Read errors",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_AUTOCHECKBOX,48,335,130,22,h,reinterpret_cast<HMENU>(Read),nullptr,nullptr);writeBox=CreateWindowW(L"BUTTON",L"Write errors",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_AUTOCHECKBOX,226,335,150,22,h,reinterpret_cast<HMENU>(Write),nullptr,nullptr);for(auto control:{readBox,writeBox})SendMessageW(control,WM_SETFONT,reinterpret_cast<WPARAM>(body),TRUE);SendMessageW(readBox,BM_SETCHECK,BST_CHECKED,0);
  if(preview){std::lock_guard<std::mutex> held(mutex);view.connected=true;view.status.Stats.Reads=1200;view.status.Stats.Writes=300;view.status.Stats.Matching=72;view.status.Stats.Injected=18;view.note=L"Offline design preview. No driver opened, no disk enumerated and no policy installed.";for(int id:{Enable,Clear,Refresh,Setup})EnableWindow(GetDlgItem(h,id),FALSE);}
  else try{worker=std::thread(Poll);}catch(...){view.note=L"The worker could not start because system resources are unavailable. No adapter or policy was opened.";}
  return 0;
 }
 case WM_APP+1:InvalidateRect(h,nullptr,FALSE);return 0;
 case WM_COMMAND:
  if(HIWORD(w)==BN_CLICKED)switch(LOWORD(w)){
   case Enable:try{unsigned flags=(SendMessageW(readBox,BM_GETCHECK,0,0)==BST_CHECKED?SF_READ:0)|(SendMessageW(writeBox,BM_GETCHECK,0,0)==BST_CHECKED?SF_WRITE:0);if(!flags)throw std::invalid_argument("Select at least Read errors or Write errors.");auto policy=StorageFault::Policy(ReadNumber(first,SF_SECTOR_COUNT),ReadNumber(count,SF_SECTOR_COUNT),static_cast<unsigned>(ReadNumber(every,1000000)),flags);if(MessageBoxW(h,L"Enable errors on the disposable RAM-backed lab disk? A mounted filesystem or application using its selected sectors can fail. No physical disk is targeted.",L"Enable lab-disk faults",MB_OKCANCEL|MB_ICONINFORMATION)==IDOK)Wake(Action::Apply,&policy);}catch(const std::exception& e){MessageBoxW(h,Wide(e.what()).c_str(),L"Invalid policy",MB_OK|MB_ICONERROR);}break;
   case Clear:Wake(Action::Stop);break;case Refresh:Wake(Action::Refresh);break;case Setup:SetupDriver();break;case Copy:CopyReport();break;
  }return 0;
 case WM_DRAWITEM:{auto item=reinterpret_cast<DRAWITEMSTRUCT*>(l);if(item->CtlType==ODT_BUTTON){bool active=item->CtlID==Enable;Fill(item->hDC,item->rcItem,(item->itemState&ODS_DISABLED)?Panel:active?Blue:RGB(44,54,70));wchar_t label[80]{};GetWindowTextW(item->hwndItem,label,80);SetBkMode(item->hDC,TRANSPARENT);SetTextColor(item->hDC,(item->itemState&ODS_DISABLED)?Muted:Ink);SelectObject(item->hDC,body);RECT r=item->rcItem;DrawTextW(item->hDC,label,-1,&r,DT_CENTER|DT_VCENTER|DT_SINGLELINE);if(item->itemState&ODS_FOCUS){InflateRect(&r,-3,-3);DrawFocusRect(item->hDC,&r);}return TRUE;}break;}
 case WM_CTLCOLOREDIT:SetTextColor(reinterpret_cast<HDC>(w),Ink);SetBkColor(reinterpret_cast<HDC>(w),Panel);return reinterpret_cast<LRESULT>(panel);
 case WM_CTLCOLORSTATIC:SetTextColor(reinterpret_cast<HDC>(w),Ink);SetBkColor(reinterpret_cast<HDC>(w),Panel);return reinterpret_cast<LRESULT>(panel);
 case WM_ERASEBKGND:return 1;
 case WM_PAINT:{PAINTSTRUCT paint{};HDC target=BeginPaint(h,&paint);RECT r{};GetClientRect(h,&r);HDC memory=CreateCompatibleDC(target);HBITMAP bitmap=CreateCompatibleBitmap(target,r.right,r.bottom);if(memory&&bitmap){auto old=SelectObject(memory,bitmap);Draw(memory,r.right,r.bottom);BitBlt(target,0,0,r.right,r.bottom,memory,0,0,SRCCOPY);SelectObject(memory,old);}if(bitmap)DeleteObject(bitmap);if(memory)DeleteDC(memory);EndPaint(h,&paint);return 0;}
 case WM_SETCURSOR:{HWND child=reinterpret_cast<HWND>(w);wchar_t type[32]{};if(child!=h&&IsWindowEnabled(child)&&GetClassNameW(child,type,32)&&_wcsicmp(type,L"BUTTON")==0){SetCursor(LoadCursorW(nullptr,IDC_HAND));return TRUE;}break;}
 case WM_CLOSE:if(!closing){closing=true;Wake(Action::Exit);if(worker.joinable())worker.join();DestroyWindow(h);}return 0;
 case WM_DESTROY:DeleteObject(body);DeleteObject(heading);DeleteObject(number);DeleteObject(background);DeleteObject(panel);PostQuitMessage(0);return 0;
 }
 return DefWindowProcW(h,message,w,l);
}
bool RenderPreview(HWND h,const wchar_t* path){
 RECT r{};GetClientRect(h,&r);HDC screen=GetDC(h),dc=CreateCompatibleDC(screen);BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=r.right;info.bmiHeader.biHeight=-r.bottom;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;void* pixels=nullptr;HBITMAP bitmap=CreateDIBSection(screen,&info,DIB_RGB_COLORS,&pixels,nullptr,0);ReleaseDC(h,screen);if(!dc||!bitmap){if(dc)DeleteDC(dc);if(bitmap)DeleteObject(bitmap);return false;}auto old=SelectObject(dc,bitmap);Draw(dc,r.right,r.bottom);
 // Draw ordinary child controls into the same deterministic, no-driver preview.
 EnumChildWindows(h,[](HWND child,LPARAM value)->BOOL{HDC image=reinterpret_cast<HDC>(value);RECT bounds{};GetWindowRect(child,&bounds);MapWindowPoints(nullptr,GetParent(child),reinterpret_cast<POINT*>(&bounds),2);int saved=SaveDC(image);SetViewportOrgEx(image,bounds.left,bounds.top,nullptr);wchar_t type[32]{};GetClassNameW(child,type,32);if(_wcsicmp(type,L"EDIT")==0){RECT field{0,0,bounds.right-bounds.left,bounds.bottom-bounds.top};Fill(image,field,RGB(44,54,70));InflateRect(&field,-1,-1);Fill(image,field,Panel);wchar_t text[32]{};GetWindowTextW(child,text,32);Text(image,8,5,field.right-12,text,body);}else SendMessageW(child,WM_PRINT,reinterpret_cast<WPARAM>(image),PRF_CLIENT|PRF_CHILDREN|PRF_ERASEBKGND);RestoreDC(image,saved);return TRUE;},reinterpret_cast<LPARAM>(dc));
 HRESULT hr=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);bool uninitialize=SUCCEEDED(hr);IWICImagingFactory* factory=nullptr;IWICStream* stream=nullptr;IWICBitmapEncoder* encoder=nullptr;IWICBitmapFrameEncode* frame=nullptr;IPropertyBag2* bag=nullptr;
 if(SUCCEEDED(hr))hr=CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory));if(SUCCEEDED(hr))hr=factory->CreateStream(&stream);if(SUCCEEDED(hr))hr=stream->InitializeFromFilename(path,GENERIC_WRITE);if(SUCCEEDED(hr))hr=factory->CreateEncoder(GUID_ContainerFormatPng,nullptr,&encoder);if(SUCCEEDED(hr))hr=encoder->Initialize(stream,WICBitmapEncoderNoCache);if(SUCCEEDED(hr))hr=encoder->CreateNewFrame(&frame,&bag);if(SUCCEEDED(hr))hr=frame->Initialize(bag);if(SUCCEEDED(hr))hr=frame->SetSize(static_cast<UINT>(r.right),static_cast<UINT>(r.bottom));for(int row=0;row<r.bottom;++row)for(int col=0;col<r.right;++col)static_cast<BYTE*>(pixels)[(row*r.right+col)*4+3]=255;WICPixelFormatGUID format=GUID_WICPixelFormat32bppBGRA;if(SUCCEEDED(hr))hr=frame->SetPixelFormat(&format);if(SUCCEEDED(hr)&&!IsEqualGUID(format,GUID_WICPixelFormat32bppBGRA))hr=E_FAIL;if(SUCCEEDED(hr))hr=frame->WritePixels(static_cast<UINT>(r.bottom),static_cast<UINT>(r.right*4),static_cast<UINT>(r.right*r.bottom*4),static_cast<BYTE*>(pixels));if(SUCCEEDED(hr))hr=frame->Commit();if(SUCCEEDED(hr))hr=encoder->Commit();if(bag)bag->Release();if(frame)frame->Release();if(encoder)encoder->Release();if(stream)stream->Release();if(factory)factory->Release();if(uninitialize)CoUninitialize();SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);return SUCCEEDED(hr);
}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR,int show){
 int countArgs=0;LPWSTR* args=CommandLineToArgvW(GetCommandLineW(),&countArgs);std::wstring render;
 if(!args)return 1;
 if(countArgs==2&&wcscmp(args[1],L"--preview")==0)preview=true;
 else if(countArgs==3&&wcscmp(args[1],L"--render-preview")==0){preview=true;render=args[2];}
 else if(countArgs!=1){LocalFree(args);MessageBoxW(nullptr,L"Use --preview for an offline synthetic interface.",L"Virtual Storage Fault Lab",MB_OK|MB_ICONERROR);return 2;}LocalFree(args);
 SetProcessDPIAware();INITCOMMONCONTROLSEX controls{sizeof(controls),ICC_STANDARD_CLASSES};InitCommonControlsEx(&controls);
 WNDCLASSEXW type{};type.cbSize=sizeof(type);type.hInstance=instance;type.lpfnWndProc=Procedure;type.lpszClassName=L"TitanStorageFaultLabWindow";type.hCursor=LoadCursorW(nullptr,IDC_ARROW);if(!RegisterClassExW(&type))return 1;
 HWND h=CreateWindowExW(0,type.lpszClassName,L"Virtual Storage Fault Lab",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,100,100,1040,755,nullptr,nullptr,instance,nullptr);if(!h)return 1;
 if(!render.empty()){bool ok=RenderPreview(h,render.c_str());DestroyWindow(h);return ok?0:1;}
 ShowWindow(h,show);MSG message{};while(GetMessageW(&message,nullptr,0,0)>0){if(!IsDialogMessageW(h,&message)){TranslateMessage(&message);DispatchMessageW(&message);}}return 0;
}
