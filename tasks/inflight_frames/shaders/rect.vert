#version 430

void main()
{
  if (gl_VertexIndex == 0) {
    gl_Position = vec4(-1.0f, -1.0f, 0.0f, 1.0f);
  } else if (gl_VertexIndex == 1 || gl_VertexIndex == 3) {
    gl_Position = vec4(1.0f, -1.0f, 0.0f, 1.0f);
  } else if (gl_VertexIndex == 2 || gl_VertexIndex == 5) {
    gl_Position = vec4(-1.0f, 1.0f, 0.0f, 1.0f);
  } else {
    gl_Position = vec4(1.0f, 1.0f, 0.0f, 1.0f);
  }
}
