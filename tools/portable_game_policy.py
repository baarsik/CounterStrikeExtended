"""Translate game compiler semantics without maintaining another source list."""
import shlex


WINDOWS_DEFINITIONS = {'WIN32', '_WIN32', '_WINDOWS', '_MBCS', '_CRT_SECURE_NO_WARNINGS',
                       '_CRT_NONSTDC_NO_WARNINGS', '_ITERATOR_DEBUG_LEVEL',
                       'LINK_MPG123_DLL', 'CSE_ENGINE_WSOCK32'}


def settings(metadata):
    optimization = metadata.get('Optimization', 'Disabled')
    optimizations = {'Disabled': '-O0', 'MaxSpeed': '-O2', 'MinSpace': '-Os', 'Full': '-O2'}
    if optimization not in optimizations:
        raise ValueError('Unsupported portable Optimization: ' + optimization)
    floating = metadata.get('FloatingPointModel', 'Precise')
    floating_options = {'Precise': ['-ffp-model=precise', '-ffp-contract=off'],
                        'Strict': ['-ffp-model=strict', '-ffp-contract=off'], 'Fast': ['-ffast-math']}
    if floating not in floating_options:
        raise ValueError('Unsupported portable floating point model: ' + floating)
    common = [optimizations[optimization], '-Wall', '-Wextra',
              '-Werror=shorten-64-to-32', '-Werror=pointer-to-int-cast'] + floating_options[floating]
    # Clang diagnoses these existing SDK conventions differently from MSVC /W4:
    # mutable legacy API strings, save offsets in polymorphic classes, intentionally
    # incomplete aggregate/switch initializers and inherited overloads. Keep their
    # diagnostics visible without changing gameplay while bringing up the platform.
    # All other warnings, including pointer narrowing, retain the owning /WX policy.
    legacy_diagnostics = ('unused-parameter', 'unknown-pragmas', 'writable-strings',
                          'overloaded-virtual', 'deprecated-copy-with-user-provided-copy',
                          'deprecated-declarations', 'missing-field-initializers',
                          'sign-compare', 'switch', 'invalid-offsetof', 'missing-braces',
                          'inconsistent-missing-override', 'nontrivial-memcall',
                          'null-arithmetic', 'null-conversion', 'logical-op-parentheses',
                          'pointer-bool-conversion', 'tautological-pointer-compare',
                          'unused-value', 'unused-but-set-variable', 'unused-private-field',
                          'delete-non-abstract-non-virtual-dtor', 'delete-abstract-non-virtual-dtor')
    common += ['-Wno-error=' + warning for warning in legacy_diagnostics]
    if floating == 'Precise':
        # Clang precise enables FMA; MSVC 2022+ precise disables it. The
        # intentional contraction override otherwise emits a driver warning.
        # https://clang.llvm.org/docs/UsersManual.html#controlling-floating-point-behavior
        # https://learn.microsoft.com/cpp/build/reference/fp-specify-floating-point-behavior
        common.append('-Wno-overriding-option')
    if metadata.get('TreatWarningAsError') == 'true':
        common.append('-Werror')
    exceptions = metadata.get('ExceptionHandling', 'false')
    fault_policy = metadata.get('CsePortableFaultPolicy')
    if fault_policy not in (None, 'OsTermination'):
        raise ValueError('Unsupported portable hardware fault policy: ' + fault_policy)
    exception_options = {'false': '-fno-exceptions', '': '-fno-exceptions', 'Sync': '-fexceptions'}
    if exceptions == 'Async':
        # Darwin C++ EH does not translate Windows SEH. Owning manifests must
        # explicitly leave fatal machine faults with the OS crash-report path.
        if fault_policy != 'OsTermination':
            raise ValueError('Async requires an explicit portable hardware fault policy')
        exception_options['Async'] = '-fexceptions'
    if exceptions not in exception_options:
        raise ValueError('Unsupported portable exception policy: ' + exceptions)
    rtti = metadata.get('RuntimeTypeInfo', 'true')
    if rtti not in ('true', 'false'):
        raise ValueError('Unsupported portable RTTI policy: ' + rtti)
    # MSVC lets an outer handler unwind through non-EH frames without running
    # their local destructors. Darwin otherwise omits those frames' unwind
    # records: an allocation exception terminates before the outer handler.
    # Keep each file's EH policy while providing the frame traversal metadata.
    cxx = [exception_options[exceptions], '-frtti' if rtti == 'true' else '-fno-rtti', '-funwind-tables']
    if metadata.get('CseObjectiveCArc') == 'true':
        cxx.append('-fobjc-arc')
        if exceptions == 'Sync':
            cxx += ['-fobjc-exceptions', '-fobjc-arc-exceptions']
    standard = metadata.get('LanguageStandard', 'stdcpp17')
    if standard not in ('stdcpp14', 'stdcpp17'):
        raise ValueError('Unsupported portable language standard: ' + standard)
    additional = shlex.split(metadata.get('AdditionalOptions', ''))
    for option in additional:
        if option not in ('/std:c++17', '/Z7', '/Zi'):
            raise ValueError('Unsupported portable AdditionalOptions: ' + option)
    debug_information = metadata.get('DebugInformationFormat', 'None')
    if debug_information not in ('None', '', 'ProgramDatabase'):
        raise ValueError('Unsupported portable debug information: ' + debug_information)
    # Shared typed callback/runtime headers require C++17 on portable compilers.
    definitions = [value for value in metadata.get('PreprocessorDefinitions', '').split(';')
                   if value and value.split('=', 1)[0] not in WINDOWS_DEFINITIONS]
    return dict(common=common, cxx=cxx, definitions=definitions, standard=17,
                symbols=debug_information == 'ProgramDatabase' or any(option in ('/Z7', '/Zi') for option in additional),
                hardware_faults='OsTermination')
