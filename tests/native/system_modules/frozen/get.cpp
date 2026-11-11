// Frozen from prerequisite 42744025bcb1a874d72dedbfce04ced8722caeaf; production method.
ISystemModule* SystemWrapper::GetModule( char* interfacename, char* library, char* instancename )
{
	library_t* lib;
	ISystemModule* module;

	if ( interfacename && interfacename[0] )
	{
		module = FindModule( interfacename, instancename );
		if ( module )
			return module;
	}

	lib = GetLibrary( library );
	if ( !lib )
		return NULL;

	module = (ISystemModule*)lib->createInterfaceFn( interfacename, NULL );
	if ( !module )
	{
		Printf( "ERROR! System::GetModule: interface \"%s\" not found in library %s.\n", interfacename, lib->name );
		return NULL;
	}

	AddModule( module, instancename );
	return module;
}
