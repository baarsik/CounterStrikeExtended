// Included inside the client compatibility CBaseEntity declaration.
// Prediction's former export check always resolved through the client stub.
// Assign the complete typed member value, including any compiler adjustment.
BASEPTR ThinkSet(BASEPTR function, const char *) {
    m_pfnThink = function;
    return function;
}

ENTITYFUNCPTR TouchSet(ENTITYFUNCPTR function, const char *) {
    m_pfnTouch = function;
    return function;
}

USEPTR UseSet(USEPTR function, const char *) {
    m_pfnUse = function;
    return function;
}

ENTITYFUNCPTR BlockedSet(ENTITYFUNCPTR function, const char *) {
    m_pfnBlocked = function;
    return function;
}
