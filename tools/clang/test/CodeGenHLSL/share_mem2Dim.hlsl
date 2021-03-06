// RUN: %dxc -E main -T cs_6_0 %s | FileCheck %s

// CHECK: addrspace(3) global [512 x float]
// CHECK: threadId
// CHECK: groupId
// CHECK: threadIdInGroup
// CHECK: flattenedThreadIdInGroup
// CHECK: addrspace(3)

groupshared column_major float2x2 dataC[8][16];
groupshared float x;

RWStructuredBuffer<float2x2> fA;

struct mat {
  row_major float2x2 f2x2;
};

StructuredBuffer<mat> mats;
StructuredBuffer<row_major float2x2> mats2;

[numthreads(8,8,1)]
void main( uint2 tid : SV_DispatchThreadID, uint2 gid : SV_GroupID, uint2 gtid : SV_GroupThreadID, uint gidx : SV_GroupIndex )
{
    if (gtid==0)
       x = tid.x;
    dataC[tid.x%8][tid.y%8] = mats.Load(gid.x).f2x2 + mats2.Load(gtid.y);
    GroupMemoryBarrierWithGroupSync();
    float2x2 f2x2 = dataC[tid.y%8][tid.x%8];

    fA[gidx] = f2x2 + x;
}
