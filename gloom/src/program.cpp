// Local headers
#include "program.hpp"
#include "OBJLoader.hpp"
#include "gloom/gloom.hpp"
#include "gloom/shader.hpp" 
#include "toolbox.hpp"
#include "sceneGraph.hpp"

#include <glm/ext.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/glm.hpp>
#include <iostream>
#include <vector>

glm::vec3 translationVector = glm::vec3(0.0);
glm::vec3 rotationVector = glm::vec3(0.0);
float rotX = 0.f;
float rotY = 0.f;
int nodeTransformation = 0;

SceneNode* nodes[8];
int numOfNodes = 6;

// chessID
unsigned int chessIndices;
unsigned int vaoChess;

float tileWidth = 30.0f;

SceneNode* scene_construction(unsigned int* IDs, int listSize, unsigned int* indexCount) {
    // Terrain
    float color1 = (0.5f,0.5f,0.5f,0.5f);
    float color2 = (0.2f,0.2f,0.2f,1.0f);
    Mesh chessboard = generateChessboard(10,10,tileWidth,color1,color2);

    float* vertex_data_chess = reinterpret_cast<float*>(&chessboard.vertices[0]);
	unsigned int* index_data_chess = reinterpret_cast<unsigned int*>(&chessboard.indices[0]);
	float* colour_data_chess = reinterpret_cast<float*>(&chessboard.colours[0]);
	vaoChess = createVertexArrayObject(vertex_data_chess,
				chessboard.vertices.size()*sizeof(float4), index_data_chess,
				chessboard.indices.size()*sizeof(unsigned int), colour_data_chess,
				chessboard.colours.size()*sizeof(float4));

    chessIndices = chessboard.indices.size()*sizeof(unsigned int);


    SceneNode* chessNode =  createSceneNode();

    SceneNode* rLegNode =  createSceneNode();
    SceneNode* lLegNode =  createSceneNode();
    SceneNode* torsoNode = createSceneNode();
    SceneNode* rArmNode =  createSceneNode();
    SceneNode* lArmNode =  createSceneNode();
    SceneNode* headNode =  createSceneNode();


    SceneNode * rootNode = createSceneNode();

    // Torso child nodes
    addChild(torsoNode, headNode);
    addChild(torsoNode, rLegNode);
    addChild(torsoNode, lLegNode);
    addChild(torsoNode, rArmNode);
    addChild(torsoNode, lArmNode);

    addChild(rootNode, chessNode);
    addChild(rootNode, torsoNode);

    // Reference point
    headNode->referencePoint = glm::vec3(0,24,0);
    torsoNode->referencePoint = glm::vec3(0,0,0);
    lArmNode->referencePoint = glm::vec3(-6,22,0);
    rArmNode->referencePoint = glm::vec3(6,22,0);
    rLegNode->referencePoint = glm::vec3(2,12,0);
    lLegNode->referencePoint = glm::vec3(6,12,0);

    chessNode->referencePoint = glm::vec3(0,0,0);

    nodes[0] = headNode;
	nodes[1] = torsoNode; 
	nodes[2] = lArmNode;
	nodes[3] = rArmNode; 
	nodes[4] = rLegNode; 
	nodes[5] = lLegNode;


	for (int i=0; i < listSize; i++) {
		std::cout << i << std::endl;
		nodes[i]->vertexArrayObjectID = IDs[i];
		nodes[i]->VAOIndexCount = indexCount[i];
	}

	nodes[6] = chessNode;
	nodes[6]->vertexArrayObjectID = vaoChess;
	nodes[6]->VAOIndexCount = chessIndices;

	nodes[7] = rootNode;

    printNode(rootNode);
    
    // Model

    return rootNode;
}

void visitSceneNode(SceneNode* node, glm::mat4 transformationThusFar) {

    // Do transformations here
	// Rotate by angle given in node, about the reference point. Using equation
	// from task 1 described in the report.
	glm::mat4 transformationMatrix = glm::translate(node->position)*glm::translate(node->referencePoint)
		* glm::rotate(node->rotation.z, glm::vec3(0,0,1))
		* glm::rotate(node->rotation.y, glm::vec3(0,1,0)) 
		* glm::rotate(node->rotation.x, glm::vec3(1,0,0))*glm::translate(-node->referencePoint);

	// Updating the matrix in the node with the new transformation matrix and
	// applying parent's transformations as well
	node->currentTransformationMatrix = transformationThusFar * transformationMatrix;

	// Need to pass the transformation over to the vertex shader as a uniform
	// variable
	glUniformMatrix4fv(nodeTransformation, 1, GL_FALSE, (float*)(&node->currentTransformationMatrix));

    // Do rendering here
	// Rendering the VAO of all the nodes that atually has a VAO (exludes the
	// rootnode of the scene, which has VAOID set to default -1)
	if (node->vertexArrayObjectID != -1) {
		glBindVertexArray(node->vertexArrayObjectID);
		glDrawElements(GL_TRIANGLES, node->VAOIndexCount, GL_UNSIGNED_INT, (void*)0);
	}

	// recursively visit all child-nodes
	glm::mat4 combinedTransformation = node->currentTransformationMatrix;
    for(SceneNode* child : node->children) {
        visitSceneNode(child, combinedTransformation);
    }
}

float timeDelta;
float timeThusFar = 0;

void animate() {
	const float a = 1.0;
	// increment the time in order to get the oscillating effect from the
	// sine-func
	timeThusFar += timeDelta;

	// Animate
	// This will make the arms and legs swing back and forth (sinus-curves). In
	// order to increase the animation, hopefully proportional to movement
	// speed, increase the constant a
	// left arm
	nodes[2]->rotation.x = a * sinf(timeThusFar);
	// right arm
	nodes[3]->rotation.x = -a * sinf(timeThusFar);
	// left leg
	nodes[4]->rotation.x = a * sinf(timeThusFar);
	// right leg 
	nodes[5]->rotation.x = -a * sinf(timeThusFar);

}

const float movementSpeed = 10.0f;
Path path = Path("coordinates_0.txt");

void move() {
	// Grab the new goal-tile from the path object
	float2 target = path.getCurrentWaypoint(tileWidth);
	glm::vec2 targetTile;
	// convert it to a vec2 to make operations later a bit easier
	targetTile.x = target.x;
	targetTile.y = target.y;
	
	SceneNode* torso = nodes[1];
	// using eucledian-distance to calculate the distanceVector (x,y) from
	// torso's current position and the targetTile's position
	// Need to normalize the the vector (transposed)
	glm::vec2 distanceVector = glm::normalize(targetTile - glm::vec2(torso->position.x, torso->position.z));

	// the translation is then proportional to the movement speed and also the
	// time elapsed. Without the time-delta of the frame, the character will
	// move from point to point instantly... 
	glm::vec2 translation = timeDelta * movementSpeed * distanceVector;

	// increment the position of the torso towards the tile. Here we use z and
	// not y because of the defined directions in the coordinate system
	// (right-hand coordinate system)
	//std::cout << translation.x << std::endl;
	torso->position.x += translation.x;
	torso->position.z += translation.y;

	float2 characterPosition;
    glm::vec2 charPos = {characterPosition.x, characterPosition.y};
	characterPosition.x = torso->position.x;
	characterPosition.y = torso->position.z;
    /* std::cout << timeDelta << std::endl; */

	// set next waypoint if current waypoint has been reached
	if (path.hasWaypointBeenReached(characterPosition, tileWidth)) {
		std::cout << "point reached" << std::endl;
		path.advanceToNextWaypoint();
	}

	// split the distance vector into its two component-vectors and grab the
	// angle between these two vectors. This is the angl between the hypotenuse
	// and the nearest side -> inverse tangent function arctan(x/y)
    torso->rotation.y = atan2(distanceVector.x, distanceVector.y);
}

int createVertexArrayObject(float* vertex_buffer_data, 
							unsigned long dataLength, 
							unsigned int* index_buffer_data, 
							unsigned long indexLength,
							float* colour_buffer_data,
							unsigned long colourLength) {

	unsigned int vertexArrayID;
	glGenVertexArrays(1, &vertexArrayID);
	glBindVertexArray(vertexArrayID);

	unsigned int vertexBufferID;
	glGenBuffers(1, &vertexBufferID);
	glBindBuffer(GL_ARRAY_BUFFER, vertexBufferID);

	glBufferData(
		GL_ARRAY_BUFFER,
		dataLength,
		vertex_buffer_data,
		GL_STATIC_DRAW
	);

	// Vertex VAP
	glVertexAttribPointer(
		0,					// layout index for shader
		4,					// 3 components, x,y,z
		GL_FLOAT,			// matches g_vertex_buffer_data
		GL_FALSE,			// normalise?
		0,					// stride, texture locations
		(void*) 0			// buffer offset
	);
	glEnableVertexAttribArray(0);

	unsigned int indexBufferID;
	glGenBuffers(1, &indexBufferID);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBufferID);
	
	glBufferData(
		GL_ELEMENT_ARRAY_BUFFER,
		indexLength,
		index_buffer_data,
		GL_STATIC_DRAW
	);

	// Generate color buffer and bind it
	unsigned int colorBufferID;
	glGenBuffers(1, &colorBufferID);
	glBindBuffer(GL_ARRAY_BUFFER, colorBufferID);

	// Pass color data to the buffer
	glBufferData(
		GL_ARRAY_BUFFER,
		colourLength,
		colour_buffer_data,
		GL_STATIC_DRAW
	);

	glVertexAttribPointer(
		1,                  // layout index for shader
		4,                  // size
		GL_FLOAT,           // type
		GL_FALSE,           // normalized?
		0,                  // stride
		(void*)0            // array buffer offset
	);
	glEnableVertexAttribArray(1);

	return vertexArrayID;
}

void render(unsigned int* IDs) {
	for (unsigned int a = 0; a < sizeof(IDs)/sizeof(IDs[0]); a = a + 1) {
		glBindVertexArray(IDs[a]);
		glDrawElements(GL_TRIANGLES, 64, GL_UNSIGNED_INT, 0);
	}
}

void runProgram(GLFWwindow* window) {

	// Enable transparancy
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// Setup shader
	printGLError();
    Gloom::Shader shader;
    shader.makeBasicShader("../gloom/shaders/simple.vert",
        "../gloom/shaders/simple.frag");
	printGLError();

    // Enable depth (Z) buffer (accept "closest" fragment)
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    // Configure miscellaneous OpenGL settings
	// Turning this on renders the insides of the minecraft character only
    glEnable(GL_CULL_FACE);
	glFrontFace(GL_CCW);

    // Set default colour after clearing the colour buffer
    glClearColor(0.3f, 0.5f, 0.8f, 1.0f);

	// Retrieve the character model	
	MinecraftCharacter character = loadMinecraftCharacterModel("../gloom/res/steve.obj");

	// Create VAOs
	float* vertex_data = reinterpret_cast<float*>(&character.head.vertices[0]);
	unsigned int* index_data = reinterpret_cast<unsigned int*>(&character.head.indices[0]);
	float* colour_data = reinterpret_cast<float*>(&character.head.colours[0]);
	unsigned int vaoHead = createVertexArrayObject(vertex_data,
				character.head.vertices.size()*sizeof(float4), index_data,
				character.head.indices.size()*sizeof(unsigned int), colour_data,
				character.head.colours.size()*sizeof(float4));

	vertex_data = reinterpret_cast<float*>(&character.torso.vertices[0]);
	index_data = reinterpret_cast<unsigned int*>(&character.torso.indices[0]);
	colour_data = reinterpret_cast<float*>(&character.torso.colours[0]);
	unsigned int vaoTorso = createVertexArrayObject(vertex_data,
				character.torso.vertices.size()*sizeof(float4), index_data,
				character.torso.indices.size()*sizeof(unsigned int), colour_data,
				character.torso.colours.size()*sizeof(float4));

	vertex_data = reinterpret_cast<float*>(&character.leftArm.vertices[0]);
	index_data = reinterpret_cast<unsigned int*>(&character.leftArm.indices[0]);
	colour_data = reinterpret_cast<float*>(&character.leftArm.colours[0]);
	unsigned int vaoLeftArm = createVertexArrayObject(vertex_data,
				character.leftArm.vertices.size()*sizeof(float4), index_data,
				character.leftArm.indices.size()*sizeof(unsigned int), colour_data,
				character.leftArm.colours.size()*sizeof(float4));

	vertex_data = reinterpret_cast<float*>(&character.rightArm.vertices[0]);
	index_data = reinterpret_cast<unsigned int*>(&character.rightArm.indices[0]);
	colour_data = reinterpret_cast<float*>(&character.rightArm.colours[0]);
	unsigned int vaoRightArm = createVertexArrayObject(vertex_data,
				character.rightArm.vertices.size()*sizeof(float4), index_data,
				character.rightArm.indices.size()*sizeof(float4), colour_data,
				character.rightArm.colours.size()*sizeof(float4));

	vertex_data = reinterpret_cast<float*>(&character.leftLeg.vertices[0]);
	index_data = reinterpret_cast<unsigned int*>(&character.leftLeg.indices[0]);
	colour_data = reinterpret_cast<float*>(&character.leftLeg.colours[0]);
	unsigned int vaoLeftLeg = createVertexArrayObject(vertex_data,
				character.leftLeg.vertices.size()*sizeof(float4), index_data,
				character.leftLeg.indices.size()*sizeof(unsigned int), colour_data,
				character.leftLeg.colours.size()*sizeof(float4));

	vertex_data = reinterpret_cast<float*>(&character.rightLeg.vertices[0]);
	index_data = reinterpret_cast<unsigned int*>(&character.rightLeg.indices[0]);
	colour_data = reinterpret_cast<float*>(&character.rightLeg.colours[0]);
	unsigned int vaoRightLeg = createVertexArrayObject(vertex_data,
				character.rightLeg.vertices.size()*sizeof(float4), index_data,
				character.rightLeg.indices.size()*sizeof(unsigned int), colour_data,
				character.rightLeg.colours.size()*sizeof(float4));


    unsigned int indexCount[6] = {character.head.indices.size()*sizeof(float4),
                                  character.torso.indices.size()*sizeof(float4),
                                  character.leftArm.indices.size()*sizeof(float4),
                                  character.rightArm.indices.size()*sizeof(float4),
                                  character.leftLeg.indices.size()*sizeof(float4),
                                  character.rightArm.indices.size()*sizeof(float4)
                                    };
	unsigned int IDs[6] = {vaoHead, vaoTorso, vaoLeftArm, vaoRightArm, vaoRightLeg, vaoLeftLeg};
    SceneNode* sceneNode = scene_construction(IDs, sizeof(IDs) / sizeof(IDs[0]), indexCount);


	std::vector<unsigned int> indecies[6] = {
								character.head.indices,
								character.torso.indices,
								character.leftArm.indices,
								character.rightArm.indices,
								character.rightLeg.indices,
								character.leftLeg.indices };

	// Sum up the indecies for drawcall
	//unsigned int indexCount = (character.head.indices.size()) / sizeof(unsigned int);

	float4 colour1 = {1.0, 1.0, 1.0, 1.0};
	float4 colour2 = {0.5, 1.0, 1.0, 0.5};

	Mesh chessBoard = generateChessboard(10.0, 10.0, 0.2, colour1, colour2);

	// Transformations
    int program_id = glGetUniformLocation(shader.shader_num(), "movePosition");
	nodeTransformation = glGetUniformLocation(shader.shader_num(), "nodeTransformation");

    glm::mat4x4 matrix;
	glm::mat4x4 identityMatrix = glm::mat4();

    // Rendering Loop
    while (!glfwWindowShouldClose(window))
    {
		// Transformation for camera movement
        matrix = glm::perspective(90.f, 1.f, 1.f, 1000.f);
        matrix = matrix*glm::translate(translationVector);
        matrix = matrix*glm::rotate(rotX, glm::vec3(0.f,1.f,0.f));
        matrix = matrix*glm::rotate(rotY, glm::vec3(1.f,0.f,0.f));

        // Clear colour and depth buffers
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		shader.activate();
        glUniformMatrix4fv(program_id, 1, GL_FALSE, (float*)(&matrix));

		// Calculate delta time for animation
		timeDelta = getTimeDeltaSeconds();

		/*int listSize = sizeof(IDs) / sizeof(IDs[0]);
		unsigned int numOfIndecies;
		for (int i = 0; i < listSize; i++) {
			glBindVertexArray(IDs[i]);
			numOfIndecies = indecies[0].size();
			//std::cout << numOfIndecies << std::endl;
			glDrawElements(GL_TRIANGLES, numOfIndecies, GL_UNSIGNED_INT, (void*)0);
		}

        glBindVertexArray(vaoChess);
        glDrawElements(GL_TRIANGLES, chessIndices, GL_UNSIGNED_INT, (void*)0);*/
		
		animate();
		move();
		visitSceneNode(sceneNode, identityMatrix);
		
		shader.deactivate();

        // Handle other events
        glfwPollEvents();
        handleKeyboardInput(window);

        // Flip buffers
        glfwSwapBuffers(window);
    }
}

void handleKeyboardInput(GLFWwindow* window)
{
    // Use escape key for terminating the GLFW window
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
		glfwSetWindowShouldClose(window, GL_TRUE);
	}

	//Rotations
	if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) {
		rotX -= 0.01;
	}

	if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) {
		rotX += 0.01;
	}

	if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) {
		rotY -= 0.01;
	}

	if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) {
		rotY += 0.01;
	}

    // Translations
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        translationVector[2] += 0.3;
	}

    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        translationVector[2] -= 0.3;
	}

    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        translationVector[0] += 0.1;
	}

    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        translationVector[0] -= 0.1;
	}

    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
        translationVector[1] -= 0.1;
	}

    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
        translationVector[1] += 0.1;
	}
}
