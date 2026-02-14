#include <GL/glut.h>
#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include "tinyxml2.h"

using namespace std;
using namespace tinyxml2;

struct Pos {
	float x, y, z;
};

struct Node {
	string text;
	vector<Node*> children;
	Pos pos;
	int size;
};

Node *root = nullptr;
GLUquadric *quad = nullptr;
float rot_x = 0.0f, rot_y = 0.0f, zoom = 20.0f;
int last_mouse_x = 0, last_mouse_y = 0;
bool vertical_mode = true;
bool proportional_layout = false;
bool animation_on = false;
float animation_angle = 0.0f;
int button;
float panX = 0.0f;
float panY = 0.0f;
bool fullScreen;

Node* parseMM(const string &filename) {

	XMLDocument doc;
	if (doc.LoadFile(filename.c_str()) != XML_SUCCESS) {
		cerr << "Failed to load " << filename << endl;
		return nullptr;
	}
	XMLElement *map = doc.FirstChildElement("map");
	if (!map)
		return nullptr;
	XMLElement *rootElem = map->FirstChildElement("node");
	if (!rootElem)
		return nullptr;

	Node *root = new Node();
	root->text = rootElem->Attribute("TEXT") ? rootElem->Attribute("TEXT") : "";

	auto parseRec = [&](auto &&self, XMLElement *elem, Node *node) -> void {
		for (XMLElement *child = elem->FirstChildElement("node"); child; child =
				child->NextSiblingElement("node")) {
			Node *newNode = new Node();
			newNode->text =
					child->Attribute("TEXT") ? child->Attribute("TEXT") : "";
			node->children.push_back(newNode);
			self(self, child, newNode);
		}
	};
	parseRec(parseRec, rootElem, root);
	return root;
}

int computeSize(Node *node) {

	if (!node)
		return 0;
	node->size = 1;
	for (auto child : node->children) {
		node->size += computeSize(child);
	}
	return node->size;
}

float findMinY(const Node *node) {

	if (!node)
		return 0.0f;
	float minY = node->pos.y;
	for (const auto *child : node->children) {
		minY = std::min(minY, findMinY(child));
	}
	return minY;
}

void shiftTree(Node *node, float dx, float dy, float dz) {

	if (!node)
		return;
	node->pos.x += dx;
	node->pos.y += dy;
	node->pos.z += dz;
	for (auto *child : node->children) {
		shiftTree(child, dx, dy, dz);
	}
}

void layoutTree(Node *node, bool vertical, bool proportional,
		float level_height = 5.0f, float base_radius_factor = 0.5f,
		float bottom_margin = 4.0f) {

	if (!node)
		return;

	// Layout assuming root at (0,0,0)
	node->pos = { 0.0f, 0.0f, 0.0f };

	auto layoutRec = [&](auto &&self, Node *curr, Pos parent_pos,
			float parent_angle, bool is_root = true) -> void {
		if (!is_root) {
			curr->pos = parent_pos;
		}
		if (curr->children.empty())
			return;

		int num_children = curr->children.size();
		if (num_children == 0)
			return;

		float total_sub = proportional ? (curr->size - 1) : num_children;
		float radius = total_sub * base_radius_factor + 1.0f;
		float cum_angle = parent_angle;

		Pos base_center = parent_pos;
		if (vertical) {
			base_center.y -= level_height;
		} else {
			base_center.x += level_height;
		}

		for (auto child : curr->children) {
			float span_weight = proportional ? child->size : 1.0f;
			float span = 2.0f * M_PI * (span_weight / total_sub);
			float child_angle = cum_angle + span / 2.0f;

			Pos child_pos = base_center;
			if (vertical) {
				child_pos.x += radius * sin(child_angle);
				child_pos.z += radius * cos(child_angle);
			} else {
				child_pos.y += radius * sin(child_angle);
				child_pos.z += radius * cos(child_angle);
			}

			self(self, child, child_pos, child_angle, false);
			cum_angle += span;
		}
	};

	layoutRec(layoutRec, node, node->pos, 0.0f, true);

	// Find lowest point and shift whole tree upward
	if (vertical) {
		float min_y = findMinY(node);
		float shift_up = -min_y + bottom_margin;
		shiftTree(node, 0.0f, shift_up, 0.0f);
	}
}

void deleteTree(Node *node) {

	if (!node)
		return;
	for (auto child : node->children) {
		deleteTree(child);
	}
	delete node;
}

void drawNode(const Node *node) {

	glPushMatrix();
	glTranslatef(node->pos.x, node->pos.y, node->pos.z);
	glColor3f(0.0f, 0.0f, 1.0f);
	glutSolidSphere(0.2f, 10, 10);

	// Draw text (simple, may not be perfectly oriented)
	glColor3f(1.0f, 1.0f, 1.0f);
	glRasterPos3f(0.3f, 0.0f, 0.0f);
	for (char c : node->text) {
		glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, c);
	}
	glPopMatrix();
}

void drawCone(const Pos &parent_pos, const Pos &base_center, float radius,
		float height, bool vertical) {

	glPushMatrix();

	// Move to the PARENT (apex/narrow end)
	glTranslatef(parent_pos.x, parent_pos.y, parent_pos.z);

	// Cone shading
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glColor4f(0.15f, 0.55f, 1.00f, 0.52f);

	GLUquadric *quad = gluNewQuadric();
	gluQuadricDrawStyle(quad, GLU_FILL);
	gluQuadricNormals(quad, GLU_SMOOTH);

	if (vertical) {
		glRotatef(90.0f, 1.0f, 0.0f, 0.0f);
		glRotatef(180.0f, 0.0f, 0.0f, 1.0f);
		glColor4f(0.15f, 0.55f, 1.00f, 0.52f);
		gluCylinder(quad, 0.0f, radius, height, 32, 1);
	} else {
		glRotatef(90.0f, 0.0f, 1.0f, 0.0f);
		glColor4f(0.15f, 0.55f, 1.00f, 0.52f);
		gluCylinder(quad, 0.0f, radius, height, 32, 1);
	}

	glPopMatrix();

	gluDeleteQuadric(quad);
	glDisable(GL_BLEND);

	// Wireframe
	glColor3f(0.4f, 0.7f, 1.0f);
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	glLineWidth(1.2f);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glColor4f(0.4f, 0.8f, 1.0f, 0.7f);

	GLUquadric *quad_wire = gluNewQuadric();
	if (vertical) {
		glPushMatrix();
		glTranslatef(parent_pos.x, parent_pos.y, parent_pos.z);
		glRotatef(90.0f, 1.0f, 0.0f, 0.0f);
		glRotatef(180.0f, 0.0f, 0.0f, 1.0f);
		gluCylinder(quad_wire, 0.0, radius, height, 32, 1);
		glPopMatrix();
	} else {
		glPushMatrix();
		glTranslatef(parent_pos.x, parent_pos.y, parent_pos.z);
		glRotatef(90.0f, 0.0f, 1.0f, 0.0f);
		gluCylinder(quad_wire, 0.0, radius, height, 32, 1);
		glPopMatrix();
	}
	gluDeleteQuadric(quad_wire);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glDisable(GL_BLEND);
}

void drawTree(const Node *node, bool vertical, float height = 5.0f) {

	drawNode(node);

	if (node->children.empty())
		return;

	float radius = (
			proportional_layout ? (node->size - 1) : node->children.size())
			* 0.5f + 1.0f;
	Pos base_center = node->pos;
	if (vertical) {
		base_center.y -= height;
	} else {
		base_center.x += height;
	}

	drawCone(node->pos, base_center, radius, height, vertical);

	for (auto child : node->children) {

		drawTree(child, vertical, height);
	}
}

void display() {

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glLoadIdentity();

	glTranslatef(panX, panY, -zoom);

	glTranslatef(0.0f, 0.0f, -zoom);
	glRotatef(rot_x, 1.0f, 0.0f, 0.0f);
	glRotatef(rot_y, 0.0f, 1.0f, 0.0f);

	if (root) {
		drawTree(root, vertical_mode);
	}

	glutSwapBuffers();
}

void reshape(int w, int h) {

	glViewport(0, 0, w, h);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(45.0, (float) w / h, 0.1, 1000.0); // Near and far clipping planes
	glMatrixMode(GL_MODELVIEW);
}

void mouse(int btn, int state, int x, int y) {

	if (state == GLUT_DOWN) {
		button = btn;
		last_mouse_x = x;
		last_mouse_y = y;
	} else if (state == GLUT_UP) {
		if (btn == 3)
			zoom = std::max(5.0f, zoom - 1.5f);
		else if (btn == 4)
			zoom += 1.5f;
		glutPostRedisplay();
	}
}

void motion(int mx, int my) {

	int dx = mx - last_mouse_x;
	int dy = my - last_mouse_y;

	if (button == GLUT_LEFT_BUTTON) {
		rot_y += dx * 0.4f;
		rot_x += dy * 0.4f;
	} else if (button == GLUT_RIGHT_BUTTON) {
		panX += dx * 0.018f;
		panY -= dy * 0.018f;
	}

	last_mouse_x = mx;
	last_mouse_y = my;
	glutPostRedisplay();
}

void keyboard(unsigned char key, int x, int y) {

	switch (key) {
	case 'v':
	case 'V':
		vertical_mode = true;
		layoutTree(root, vertical_mode, proportional_layout);
		break;
	case 'h':
	case 'H':
		vertical_mode = false;
		layoutTree(root, vertical_mode, proportional_layout);
		break;
	case 'p':
	case 'P':
		proportional_layout = !proportional_layout;
		layoutTree(root, vertical_mode, proportional_layout);
		break;
	case 'a':
	case 'A':
		animation_on = !animation_on;
		break;
	case 'f':
	case 'F':
		if (!fullScreen) {
			glutFullScreen();
			fullScreen = true;
		} else if (fullScreen) {
			glutReshapeWindow(800, 600);
			glutPositionWindow(0, 0);
			fullScreen = false;
		}
		break;
	case 27: // ESC
		deleteTree(root);
		gluDeleteQuadric(quad);
		exit(0);
	}
	glutPostRedisplay();
}

void timer(int value) {

	if (animation_on) {
		if (vertical_mode) {
			rot_y += 1.0f;
		} else {
			rot_x += 1.0f;
		}
		glutPostRedisplay();
	}
	glutTimerFunc(20, timer, 0);
}

int main(int argc, char **argv) {

	if (argc < 2) {
		cerr << "Usage: " << argv[0] << " mindmap.mm" << endl;
		return 1;
	}
	string filename = argv[1];
	root = parseMM(filename);
	if (!root)
		return 1;
	computeSize(root);
	layoutTree(root, vertical_mode, proportional_layout);

	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
	glutInitWindowSize(800, 600);
	glutCreateWindow("ConeTree Viewer");

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	quad = gluNewQuadric();
	gluQuadricDrawStyle(quad, GLU_FILL);

	glutDisplayFunc(display);
	glutReshapeFunc(reshape);
	glutMouseFunc(mouse);
	glutMotionFunc(motion);
	glutKeyboardFunc(keyboard);
	glutTimerFunc(20, timer, 0);

	glutMainLoop();
	return 0;
}
