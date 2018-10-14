#version 430 core

in layout(location=0) vec4 position;
in layout(location=1) vec4 vertexColor;
uniform mat4x4 movePosition;
uniform mat4x4 nodeTransformation;

out vec4 fragmentColor;

void main()
{
	// Transformation
	gl_Position = movePosition*nodeTransformation*position;

	// produce color of each vertex
	fragmentColor = vertexColor;
}
