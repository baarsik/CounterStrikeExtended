#pragma once

// Included by the owning game utility TU after its entvars descriptions and engine callbacks.
// The standalone native fixture supplies the same SDK types and owned callback/data boundaries.
void EntvarsKeyvalue(entvars_t *pev, KeyValueData *pkvd) {
    for (int index = 0; index < ENTVARS_COUNT; ++index) {
        const auto &field = gEntvarsDescription[index];
        if (!stricmp(field.fieldName, pkvd->szKeyName)) {
            char *storage = reinterpret_cast<char *>(pev) + field.fieldOffset;
            switch (field.fieldType) {
            case FIELD_MODELNAME:
            case FIELD_SOUNDNAME:
            case FIELD_STRING:
                *reinterpret_cast<string_t *>(storage) = ALLOC_STRING(pkvd->szValue);
                break;

            case FIELD_TIME:
            case FIELD_FLOAT:
                *reinterpret_cast<float *>(storage) = static_cast<float>(atof(pkvd->szValue));
                break;

            case FIELD_INTEGER:
                *reinterpret_cast<int *>(storage) = atoi(pkvd->szValue);
                break;

            case FIELD_POSITION_VECTOR:
            case FIELD_VECTOR:
                UTIL_StringToVector(reinterpret_cast<float *>(storage), pkvd->szValue);
                break;

            default:
            case FIELD_EVARS:
            case FIELD_CLASSPTR:
            case FIELD_EDICT:
            case FIELD_ENTITY:
            case FIELD_POINTER:
                ALERT(at_error, const_cast<char *>("Bad field in entity!!\n"));
                break;
            }
            pkvd->fHandled = TRUE;
            return;
        }
    }
}
