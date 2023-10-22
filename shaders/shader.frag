#version 450

layout(location = 0) in vec3 fragColor;

layout(location = 1) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

layout(binding = 1) uniform sampler2D texSampler;

void main() {

// Just like the per vertex colors, the fragTexCoord values will be smoothly interpolated across the area of the square by the rasterizer. 
// We can visualize this by having the fragment shader output the texture coordinates as colors
// The green channel represents the horizontal coordinates and the red channel the vertical coordinates.
// The black and yellow corners confirm that the texture coordinates are correctly interpolated from 0, 0 to 1, 1 across the square.
// Visualizing data using colors is the shader programming equivalent of printf debugging, for lack of a better option!

    //outColor = vec4(fragTexCoord, 0.0, 1.0);

    // the result in the image when using VK_SAMPLER_ADDRESS_MODE_REPEAT
    //outColor = texture(texSampler, fragTexCoord * 2.0);

    // separated the RGB and alpha channels here to not scale the alpha channel.
    //outColor = vec4(fragColor * texture(texSampler, fragTexCoord).rgb, 1.0);

    outColor = texture(texSampler, fragTexCoord);
}