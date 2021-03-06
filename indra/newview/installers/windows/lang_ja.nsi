; First is default
LoadLanguageFile "${NSISDIR}\Contrib\Language files\Japanese.nlf"

; Language selection dialog
LangString InstallerLanguageTitle  ${LANG_JAPANESE} "インストーラの言語"
LangString SelectInstallerLanguage  ${LANG_JAPANESE} "インストーラの言語を選択してください"

; subtitle on license text caption
LangString LicenseSubTitleUpdate ${LANG_JAPANESE} " アップデート" 
LangString LicenseSubTitleSetup ${LANG_JAPANESE} " セットアップ" 

; description on license page
LangString LicenseDescUpdate ${LANG_JAPANESE} "このパッケージはFirestormをバージョン${VERSION_LONG}.にアップデートします。" 
LangString LicenseDescSetup ${LANG_JAPANESE} "このパッケージはあなたのコンピュータにFirestormをインストールします。" 
LangString LicenseDescNext ${LANG_JAPANESE} "次" 

; installation directory text
LangString DirectoryChooseTitle ${LANG_JAPANESE} "インストール・ディレクトリ" 
LangString DirectoryChooseUpdate ${LANG_JAPANESE} "アップデートするFirestormのディレクトリを選択してください。:" 
LangString DirectoryChooseSetup ${LANG_JAPANESE} "Firestormをインストールするディレクトリを選択してください。: " 

; CheckStartupParams message box
LangString CheckStartupParamsMB ${LANG_JAPANESE} "プログラム名'$INSTPROG'が見つかりません。サイレント・アップデートに失敗しました。" 

; installation success dialog
LangString InstSuccesssQuestion ${LANG_JAPANESE} "直ちにFirestormを開始しますか？ " 

; remove old NSIS version
LangString RemoveOldNSISVersion ${LANG_JAPANESE} "古いバージョン情報をチェック中です…" 

; check windows version
LangString CheckWindowsVersionDP ${LANG_JAPANESE} "ウィンドウズのバージョン情報をチェック中です..." 
LangString CheckWindowsVersionMB ${LANG_JAPANESE} "FirestormはWindows XP、Mac OS Xのみをサポートしています。Windows $R0をインストールする事は、データの消失やクラッシュの原因になる可能性があります。インストールを続けますか？" 
LangString CheckWindowsServPackMB ${LANG_JAPANESE} "It is recomended to run Firestorm on the latest service pack for your operating system.$\nThis will help with performance and stability of the program."
LangString UseLatestServPackDP ${LANG_JAPANESE} "Please use Windows Update to install the latest Service Pack."

; checkifadministrator function (install)
LangString CheckAdministratorInstDP ${LANG_JAPANESE} "インストールのための権限をチェック中です..." 
LangString CheckAdministratorInstMB ${LANG_JAPANESE} "Firestormをインストールするには管理者権限が必要です。"

; checkifadministrator function (uninstall)
LangString CheckAdministratorUnInstDP ${LANG_JAPANESE} "アンインストールのための権限をチェック中です..." 
LangString CheckAdministratorUnInstMB ${LANG_JAPANESE} "Firestormをアンインストールするには管理者権限が必要です。" 

; checkifalreadycurrent
LangString CheckIfCurrentMB ${LANG_JAPANESE} "Firestorm${VERSION_LONG} はインストール済みです。再度インストールしますか？ " 

; checkcpuflags
LangString MissingSSE2 ${LANG_JAPANESE} "This machine may not have a CPU with SSE2 support, which is required to run SecondLife ${VERSION_LONG}. Do you want to continue?"

; closesecondlife function (install)
LangString CloseSecondLifeInstDP ${LANG_JAPANESE} "Firestormを終了中です..." 
LangString CloseSecondLifeInstMB ${LANG_JAPANESE} "Firestormの起動中にインストールは出来ません。直ちにFirestormを終了してインストールを開始する場合はOKボタンを押してください。CANCELを押すと中止します。"

; closesecondlife function (uninstall)
LangString CloseSecondLifeUnInstDP ${LANG_JAPANESE} "Firestormを終了中です..." 
LangString CloseSecondLifeUnInstMB ${LANG_JAPANESE} "Firestormの起動中にアンインストールは出来ません。直ちにFirestormを終了してアンインストールを開始する場合はOKボタンを押してください。CANCELを押すと中止します。" 

; CheckNetworkConnection
LangString CheckNetworkConnectionDP ${LANG_JAPANESE} "ネットワークの接続を確認中..." 

; removecachefiles
LangString RemoveCacheFilesDP ${LANG_JAPANESE} " Documents and Settings フォルダのキャッシュファイルをデリート中です。" 

; delete program files
LangString DeleteProgramFilesMB ${LANG_JAPANESE} "Firestormのディレクトリには、まだファイルが残されています。$\n$INSTDIR$\nにあなたが作成、または移動させたファイルがある可能性があります。全て削除しますか？ " 

; uninstall text
LangString UninstallTextMsg ${LANG_JAPANESE} "Firestorm${VERSION_LONG}をアンインストールします。"

; <FS:Ansariel> Optional start menu entry
LangString CreateStartMenuEntry ${LANG_JAPANESE} "Create an entry in the start menu?"

LangString DeleteDocumentAndSettingsDP ${LANG_JAPANESE} "Deleting files in Documents and Settings folder."
LangString UnChatlogsNoticeMB ${LANG_JAPANESE} "This uninstall will NOT delete your Firestorm chat logs and other private files. If you want to do that yourself, delete the Firestorm folder within your user Application data folder."
LangString UnRemovePasswordsDP ${LANG_JAPANESE} "Removing Firestorm saved passwords."
