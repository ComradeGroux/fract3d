#version 460

void	main()
{
	int	u = ((gl_VertexIndex << 1) & 2);
	int	v = (gl_VertexIndex & 2);
	gl_Position = vec4(u * 2.0 - 1.0, v * 2.0 - 1.0, 0.0, 1.0);
}
