#version 450

layout(binding = 1) uniform sampler2D texSampler;

layout(location = 0) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform Image {
    vec2 size;
} push;

void main() {
  // Box filter

  const vec2 texOffset = vec2(1.0/push.size.x, 1.0/push.size.y); //vec2(1.0/512.0, 1.0/512.0);
  const float wt = 1.0/9.0;

  vec2 tc0 = fragTexCoord + vec2(-texOffset.s, -texOffset.t);
  vec2 tc1 = fragTexCoord + vec2(0.0, -texOffset.t);
  vec2 tc2 = fragTexCoord + vec2(+texOffset.s, -texOffset.t);
  vec2 tc3 = fragTexCoord + vec2(-texOffset.s, 0.0);
  vec2 tc4 = fragTexCoord + vec2(0.0, 0.0);
  vec2 tc5 = fragTexCoord + vec2(+texOffset.s, 0.0);
  vec2 tc6 = fragTexCoord + vec2(-texOffset.s, +texOffset.t);
  vec2 tc7 = fragTexCoord + vec2(0.0, +texOffset.t);
  vec2 tc8 = fragTexCoord + vec2(+texOffset.s, +texOffset.t);

  vec4 col0 = texture(texSampler, tc0);
  vec4 col1 = texture(texSampler, tc1);
  vec4 col2 = texture(texSampler, tc2);
  vec4 col3 = texture(texSampler, tc3);
  vec4 col4 = texture(texSampler, tc4);
  vec4 col5 = texture(texSampler, tc5);
  vec4 col6 = texture(texSampler, tc6);
  vec4 col7 = texture(texSampler, tc7);
  vec4 col8 = texture(texSampler, tc8);

  vec4 sum = (wt * col0 + wt * col1 + wt * col2 + wt * col3 + wt * col4 +
              wt * col5 + wt * col6 + wt * col7 + wt * col8);
  outColor = vec4(sum.rgb, 1.0);
}
