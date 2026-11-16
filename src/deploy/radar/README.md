# NextClient radar map corrections

Pinned source: CS-NextClient/NextClient commit
`1d72f0a5f576ffd0e99207ac4edfec7468c6c218`, `assets/cstrike/overviews/`.

The site-name hints and layer corrections accompany the native radar port.
The CS client target stages these files under `cstrike/overviews/` on every
supported runtime platform and configuration, restoring missing files without
forcing a relink. See `src/cstrike/cl_dll/nextclient/UPSTREAM.md` for provenance.
