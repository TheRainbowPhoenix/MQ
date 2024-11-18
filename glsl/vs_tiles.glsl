layout(location=0) in vec2 a_vertex;

out vec2 v_vertex;
out float v_scaling;

uniform mat3 u_pixel2gl;
uniform vec3 u_view;

void main()
{
    v_vertex = (a_vertex / u_view.z) + u_view.xy;
    v_scaling = u_view.z;
    gl_Position = vec4(u_pixel2gl * vec3(a_vertex, 1.0), 1.0);
}
