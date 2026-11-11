// Frozen from prerequisite 42744025bcb1a874d72dedbfce04ced8722caeaf; production method.
bool SystemWrapper::Init( IBaseSystem* system, int serial, char* name )
{
	BaseSystemModule::Init( system, serial, name );

	m_Commands.Init();
	m_Modules.Init();
	m_Libraries.Init();
	m_Listener.Init();

	m_EngineWrapper = new EngineWrapper();
#if defined( _WIN32 )
	AddModule( m_EngineWrapper, "enginewrapper002" );
#endif

	m_State = MODULE_RUNNING;
	return true;
}
