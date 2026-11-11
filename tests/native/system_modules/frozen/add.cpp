// Frozen from prerequisite 42744025bcb1a874d72dedbfce04ced8722caeaf; production method.
bool SystemWrapper::AddModule( ISystemModule* module, char* name )
{
	if ( !module )
		return false;

	if ( !module->Init( this, (int)m_SerialCounter, name ) )
	{
		Printf( "ERROR! System::AddModule: couldn't initialize module %s.\n", name );
		return false;
	}

	m_Modules.AddHead( module );
	m_SerialCounter++;
	return true;
}
