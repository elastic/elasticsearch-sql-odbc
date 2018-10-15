// break at 120 columns

#include <Windows.h>
#include <string>
#include <assert.h>
#include <vcclr.h>

#include "EsOdbcDsnBinding.h"

/* ODBC's unprefixed define is 512 */
#define _SQL_MAX_MESSAGE_LENGTH	1024

using namespace System;
using namespace System::Reflection;
using namespace System::Text;
using namespace System::Runtime::InteropServices;
using namespace System::Threading;

using namespace EsOdbcDsnEditor;


namespace EsOdbcDsnBinding {

	/*
	 * Proxy/Bridge class for the C# actual implementation.
	 */
	public ref class EsOdbcDsnBinding
	{
		bool onConnect;
		String ^dsnInStr;
		int dsnOutLen;

		driver_callback_ft cbConnectionTest;
		void *argConnectionTest;

		driver_callback_ft cbSaveDsn;
		void *argSaveDsn;

		public:static HWND hwnd;
		

		private:int cbImplementation(driver_callback_ft cbFunction, void *cbArgument,
			String ^connString, String ^%errorMessage, unsigned int flags)
		{
			assert(cbFunction);
			if (connString->Length <= 0) {
				// this should not happen, but if it does, shortcut it here.
				return 0;
			}

			// the callback understands wchar_t* only, so need to convert the input String
			pin_ptr<const wchar_t> wch = PtrToStringChars(connString);
			//buffer to hold the (possible) error message
			wchar_t errorMessageW[_SQL_MAX_MESSAGE_LENGTH] = { 0 };

			int ret = cbFunction(cbArgument, (const wchar_t *)wch,
				errorMessageW, sizeof(errorMessageW) / sizeof(*errorMessageW), flags);

			// if there was an error/warning, let the caller know of it
			if (0 < wcslen(errorMessageW)) {
				// The driver should only return errors that are actionable by the user.
				// Error message usage is dictated by error code: might be discarded for some "expected" errors.
				errorMessage = gcnew String(errorMessageW);
			}
			
			return ret;
		}
		/* 
		 * proxy method for the connection test call back
		 */
		private:int proxyConnectionTest(String ^connString, String ^%errorMessage, unsigned int flags)
		{
			return cbImplementation(cbConnectionTest, argConnectionTest, connString, errorMessage, flags);
		}

		/*
		 * proxy method for the connection test call back
		 */
		private:int proxySaveDsn(String ^connString, String ^%errorMessage, unsigned int flags)
		{
			return cbImplementation(cbSaveDsn, argSaveDsn, connString, errorMessage, flags);
		}
		
		/*
		 * The constructor merely saves the input parameters and registers an assembly resolve handler
		 */
		public: EsOdbcDsnBinding(HWND hwnd, bool onConnect, wchar_t *dsnInW,
			driver_callback_ft cbConnectionTest, void *argConnectionTest,
			driver_callback_ft cbSaveDsn, void *argSaveDsn)
		{
			/* there should(?) be one window handler in use for one application through the driver */
			assert(this->hwnd == NULL);
			this->hwnd = hwnd;

			this->onConnect = onConnect;
			dsnInStr = dsnInW != NULL ? gcnew String(dsnInW) : "";

			this->cbConnectionTest = cbConnectionTest;
			this->argConnectionTest = argConnectionTest;

			this->cbSaveDsn = cbSaveDsn;
			this->argSaveDsn = argSaveDsn;

			// register an event handler for failed assembly loading
			AppDomain::CurrentDomain->AssemblyResolve += gcnew ResolveEventHandler(resolveEventHandler);
		}

		public: ~EsOdbcDsnBinding()
		{
			hwnd = NULL;
		}

		/*
		 * Handler called if loading an assembly fails.
		 */
		static Assembly^ resolveEventHandler(Object^ sender, ResolveEventArgs^ args)
		{
			Assembly^ assembly;
			try {
				// TODO: check if this is safe "enough" ('args' always there, with proper format)
				String^ loadingAssembleyName = args->Name->Split(',')[0];

				if (loadingAssembleyName->EndsWith("resources")) {
					// no resources available to load
					assembly = nullptr;
				} else {
					// Get the bridging assembley, its location and based on that build the loading assembly path.
					// Note: this assumes that the two libraries are always going to be collocated.
					assembly = Assembly::GetExecutingAssembly();
					int lastBackSlash = assembly->Location->LastIndexOf('\\');
					String ^loadingAssemblyPath = assembly->Location->Substring(0, lastBackSlash + 1) +
						loadingAssembleyName + ".dll";

					if (!System::IO::File::Exists(loadingAssemblyPath))	{
						throw gcnew Exception("Failed to load assembly '" + loadingAssemblyPath + "'.");
					}

					// then load the assembley
					assembly = Assembly::LoadFrom(loadingAssemblyPath);
				}

				return assembly;
			} catch (Exception ^e) {
				if (hwnd) {
					pin_ptr<const wchar_t> wch = PtrToStringChars(e->Message);
					MessageBox(hwnd, wch, L"Loading Exception", MB_OK | MB_ICONERROR);
				}
				assembly = nullptr;
			}

			return assembly;
		}

		public:int EsOdbcDsnEditor()
		{
			Thread ^ t;
			/* (Re)set the threading model; "Multiple calls to CoInitializeEx by the same
			   thread are allowed as long as they pass the same concurrency flag".
			   For neutral/MTAs create a new thread, for STAs just run the worker. */
			switch (CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_SPEED_OVER_MEMORY)) {
				case S_OK:
				case S_FALSE:
					DsnEditorWorker();
					break;
				case RPC_E_CHANGED_MODE:
					t = gcnew Thread(gcnew ThreadStart(this, &EsOdbcDsnBinding::DsnEditorWorker));
					t->ApartmentState = ApartmentState::STA;
					t->Start();
					t->Join();
					break;
				default:
					throw gcnew Exception("setting threading model failed.");
			}

			return dsnOutLen;
		}

		private:void DsnEditorWorker()
		{
			EsOdbcDsnEditor::DriverCallbackDelegate ^delegConnectionTest;
			EsOdbcDsnEditor::DriverCallbackDelegate ^delegSaveDsn;

			delegConnectionTest = gcnew DriverCallbackDelegate(this, &EsOdbcDsnBinding::proxyConnectionTest);
			delegSaveDsn = gcnew DriverCallbackDelegate(this, &EsOdbcDsnBinding::proxySaveDsn);

			dsnOutLen = DsnEditorFactory::DsnEditor(onConnect, dsnInStr, delegConnectionTest, delegSaveDsn);
		}


	};
}

#ifdef __cplusplus
extern "C"
#endif /* __cpluplus */
#ifdef _WINDLL
__declspec(dllexport)
#else /* _WINDLL */
__declspec(dllimport)
#endif /* _WINDLL */
int EsOdbcDsnEdit(HWND hwnd, BOOL onConnect, wchar_t *dsnInW,
	driver_callback_ft cbConnectionTest, void *argConnectionTest,
	driver_callback_ft cbSaveDsn, void *argSaveDsn)
{
	try {
		EsOdbcDsnBinding::EsOdbcDsnBinding binding(hwnd, onConnect, dsnInW,
			cbConnectionTest, argConnectionTest,
			cbSaveDsn, argSaveDsn);
		return binding.EsOdbcDsnEditor();
	} catch (Exception^ e) {
		if (hwnd) {
			pin_ptr<const wchar_t> wch = PtrToStringChars(e->Message);
			if (! MessageBox(hwnd, wch, L"Loading Exception", MB_OK | MB_ICONERROR)) {
				// failed to display failure error
				return -3;
			}
			// failure error presented to user
			return -1;
		}
		// no window hander available
		return -2;
	}
}

