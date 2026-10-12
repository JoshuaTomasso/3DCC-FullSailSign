// an ultra simple hlsl vertex shader
// TODO: Part 1f
// TODO: Part 2c // TODO: Part 4d
cbuffer SHADER_SCENE_DATA : register(b0)
{
    float4 lightDirection; // Direction of the light source
    float4 lightColor; // Color of the light
    float4x4 viewMatrix; // Camera view transformation
    float4x4 projectionMatrix; // Camera projection transformation
    float4 eyePosition;
};
// TODO: Part 3b

// Material attributes structure - matches C++ OBJ_ATTRIBUTES layout
struct OBJ_ATTRIBUTES
{
    float3 Kd; // diffuse reflectivity
    float d; // dissolve (transparency) 
    float3 Ks; // specular reflectivity
    float Ns; // specular exponent
    float3 Ka; // ambient reflectivity
    float sharpness; // local reflection map sharpness
    float3 Tf; // transmission filter
    float Ni; // optical density (index of refraction)
    float3 Ke; // emissive reflectivity
    uint illum; // illumination model
};

// Instance data structure - matches C++ INSTANCE_DATA
struct INSTANCE_DATA
{
    float4x4 worldMatrix;
    OBJ_ATTRIBUTES material;
};
// TODO: Part 3c
// Dynamic buffer of instance data
StructuredBuffer<INSTANCE_DATA> DrawInfo : register(b1, space0);
// TODO: Part 4a
// TODO: Part 4b
// TODO: Part 3g
// TODO: Part 3h
// Output structure - must match fragment shader input
struct VS_OUTPUT
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL0;
    float3 uvw : TEXCOORD0;
    float3 worldPosition : TEXCOORD2;
    nointerpolation uint instanceID : TEXCOORD1;
};

// TODO: Part 1h
// TODO: Part 3g
// TODO: Part 2f
// TODO: Part 3h
// TODO: Part 4b
VS_OUTPUT main(float3 inputVertex : POSITION, float3 inputNormal : NORMAL, float3 inputUVW : TEXCOORD0, uint instanceID : SV_InstanceID)
{
    VS_OUTPUT output;
    
    // Get the world matrix for this specific instance/mesh
    float4x4 worldMatrix = DrawInfo[instanceID].worldMatrix;
    
    // Transform vertex from model space to world space using the instance's world matrix
    float4 worldPosition = mul(worldMatrix, float4(inputVertex, 1.0f));
    
    // Store world position for specular calculations
    output.worldPosition = worldPosition.xyz;
    
    // Transform from world space to view space
    float4 viewPosition = mul(viewMatrix, worldPosition);
    
    // Finally transform to clip space
    output.position = mul(projectionMatrix, viewPosition);
    
    // Transform normal by world matrix (excluding translation)
    output.normal = mul((float3x3) worldMatrix, inputNormal);
    output.uvw = inputUVW;
    
    // Pass the instance ID to the fragment shader
    output.instanceID = instanceID;
    
    return output;
}