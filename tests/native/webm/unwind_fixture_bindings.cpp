// The generated engine command force-includes its Steam registration constructor.
// This isolated unwind experiment records that linkage only; it never loads a provider.
volatile unsigned int fixtureSteamRegistrationCalls = 0;

extern "C" void CseSteamModuleInitialize() {
    ++fixtureSteamRegistrationCalls;
}
