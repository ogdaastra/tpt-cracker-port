luaconsole_files = files(
	'LegacyLuaAPI.cpp',
	'LuaBit.cpp',
	'LuaButton.cpp',
	'LuaCheckbox.cpp',
	'LuaCompat.c',
	'LuaComponent.cpp',
	'LuaLabel.cpp',
	'LuaProgressBar.cpp',
	'LuaScriptInterface.cpp',
	'LuaSlider.cpp',
	'LuaSmartRef.cpp',
	'LuaTextbox.cpp',
	'LuaWindow.cpp',
)
# Without HTTP, LuaTCPSocket.cpp still provides socket.gettime/socket.sleep (no socket.tcp). The mod's
# manager.lua refuses to run without the socket table, and crackerk.lua needs MANAGER for its settings.
if enable_http or host_platform == 'emscripten'
	luaconsole_files += files(
		'LuaTCPSocket.cpp',
	)
endif

subdir('luascripts')

powder_files += luaconsole_files
