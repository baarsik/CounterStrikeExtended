#pragma once

#include "const.h"
#include <cstddef>
#include "gl_model.h"

void		CM_CalcPAS( model_t* model );
void		CM_FreePAS();
void CM_InvalidatePAS(const model_t* model);
byte*		CM_LeafPVS( int leafnum );
byte*		CM_LeafPAS( int leafnum );
bool CM_DecompressPVS(const byte* input, std::size_t inputBytes, byte* decompressed, std::size_t byteCount);
qboolean	CM_HeadnodeVisible( mnode_t* node, byte* visibility, int* firstVisibleLeaf );
