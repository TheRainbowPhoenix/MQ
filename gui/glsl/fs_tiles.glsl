in vec2 v_vertex;
in float v_scaling;

out vec4 color;

/* Diamond diagonal size (pixels) */
const float diamond_size = 80.0;
/* Pattern opacity */
const float alpha = 0.015;

void main(void)
{
    float x = mod(v_vertex.x / (diamond_size * 2.0), 1.0);
    float y = mod(v_vertex.y / (diamond_size * 2.0), 1.0);

    float d = abs(x - 0.5) + abs(y - 0.5);
    float delta = 0.5 / v_scaling / diamond_size;

    vec4 bg = vec4(0.1, 0.12, 0.14, 1.0);
    vec4 fg = vec4(1.0, 1.0, 1.0, 1.0);

    vec4 c = mix(bg, fg, alpha * smoothstep(0.5-delta, 0.5+delta, d));
	color = c;
}
