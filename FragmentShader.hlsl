// an ultra simple hlsl fragment shader
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
// Storage buffer with instance data (binding 1)
StructuredBuffer<INSTANCE_DATA> instanceData : register(t1);
// TODO: Part 4a (optional)
// TODO: Part 4b
// TODO: Part 3g
// TODO: Part 3h
// Input from the vertex shader
struct PS_INPUT
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL0;
    float3 uvw : TEXCOORD0;
    float3 worldPosition : TEXCOORD2;
    nointerpolation uint instanceID : TEXCOORD1;
};
float4 main(PS_INPUT input) : SV_TARGET
{
    // Get material for this pixel
    OBJ_ATTRIBUTES material = instanceData[input.instanceID].material;

    // Normalize the interpolated normal
    float3 normalizedNormal = normalize(input.normal);

    // Get normalized light direction with correct negation
    float3 normalizedLightDir = normalize(-lightDirection.xyz);

    // Calculate diffuse component
    float diffuseFactor = max(dot(normalizedNormal, normalizedLightDir), 0.0);
    float3 diffuseColor = material.Kd * diffuseFactor * lightColor.rgb;

    // Calculate specular component using Blinn-Phong model
    float3 viewDir = normalize(eyePosition.xyz - input.worldPosition);
    float3 halfVector = normalize(normalizedLightDir + viewDir);
    float specFactor = pow(max(dot(normalizedNormal, halfVector), 0.0), material.Ns);
    float3 specularColor = material.Ks * specFactor * lightColor.rgb;

    // Add ambient component
    float3 ambientColor = material.Ka * 0.2;
    
    // Combine all lighting components
    float3 finalColor = ambientColor + diffuseColor + specularColor;
    
    return float4(finalColor, 1.0);
}