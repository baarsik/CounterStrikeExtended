"""Prove unused engine bindings abort, including the actual player's entry path."""
import subprocess
import sys

bindings = ("PlayVideo", "Con_Printf", "Con_DPrintf", "S_Update", "S_StartDynamicSound",
            "S_PrecacheSound", "Sys_FloatTime", "Sys_Sleep")
for binding in bindings:
    result = subprocess.run([sys.argv[1], "--presentation-guard", binding],
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, check=False, timeout=20)
    expected = "Con_Printf" if binding == "PlayVideo" else binding
    marker = f"UNEXERCISED_PRESENTATION_ABORT:{expected}".encode()
    assert result.returncode != 0 and marker in result.stdout, (binding, result.returncode, result.stdout)
    print(f"{binding}: abort exit={result.returncode}; binding={expected}")
