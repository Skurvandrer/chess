#include "GL/freeglut.h"
#include <SOIL/SOIL.h>
#include "GL/gl.h"

#define WHITECOL 1, 1, 0.8
#define BLACKCOL 0.3, 0.5, 0.25

extern const char _binary_image_png_start[];
extern const char _binary_image_png_end[];

void draw();
void onMouseClick(int button, int state, int pixelX, int pixelY);
void onMouseMove(int x, int y);
GLuint loadMemTexture();
void glutUpdateFunc(int value);
void update();

GLuint texture;
int WINDOWSIZE = 600;
//float mouseX;
//float mouseY;

GLuint loadMemTexture()
{
	// decode image in memory into opengl texture
	size_t image_len = _binary_image_png_end - _binary_image_png_start;
	GLuint t = SOIL_load_OGL_texture_from_memory(
        	_binary_image_png_start,
        	image_len,
        	SOIL_LOAD_AUTO, SOIL_CREATE_NEW_ID, SOIL_FLAG_MIPMAPS);

	glBindTexture(GL_TEXTURE_2D, t);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	return t;
}

void draw()
{
	glClearColor(0.0, 0.0, 0.0, 0.0);
	glClear(GL_COLOR_BUFFER_BIT);
	
	glColor3f(1.0, 1.0, 1.0);
	glOrtho(-1.0, 1.0, -1.0, 1.0, -1.0, 1.0);

	float width = 2.0 / GRIDSIZE;			
	float spriteWidth = 1.0 / 6;
	float spriteHeight = 0.5;

	//draw the tiles and pieces
	glEnable(GL_TEXTURE_2D);
	glBegin(GL_QUADS);
	for(int x=0; x<GRIDSIZE; x++) {
		for(int y=0; y<GRIDSIZE; y++) {
			int pieceCol = 1 & (grid[x][y] >> 6);
			float tileCol = (x+y+is_server+1)%2;
			if (tileCol)
			{
				//glColor4f(WHITECOL, 1);
				glColor3f(WHITECOL);
			}
			else
			{
				glColor3f(BLACKCOL);
				//glColor4f(BLACKCOL, 1);
			}

			float root_x = -1.0 + width * x;
			float root_y = -1.0 + width * y;
			float root_x_tex = spriteWidth * ((grid[x][y] & 15) - 1);
			float root_y_tex = spriteHeight * (1 - pieceCol);
			
			if (grid[x][y] & 15) {	            
				glTexCoord2f(root_x_tex, root_y_tex+spriteHeight); glVertex2f(root_x, root_y);
				glTexCoord2f(root_x_tex, root_y_tex); glVertex2f(root_x, root_y+width);
				glTexCoord2f(root_x_tex+spriteWidth, root_y_tex); glVertex2f(root_x+width, root_y+width);
				glTexCoord2f(root_x_tex+spriteWidth, root_y_tex+spriteHeight); glVertex2f(root_x+width, root_y);
			}
			else {
				glVertex2f(root_x, root_y);
				glVertex2f(root_x, root_y+width);
				glVertex2f(root_x+width, root_y+width);
				glVertex2f(root_x+width, root_y);
			}
		}
	}
	glEnd();
	glDisable(GL_TEXTURE_2D);

	// draw cursor
	float root_x = -1.0 + width * cursor.x;
	float root_y = -1.0 + width * cursor.y;

	glLineWidth(3.0);
	glColor3f(1.0, 0.0, 0.0);
	glBegin(GL_LINE_LOOP);
	glVertex3f(root_x	  , root_y	   , 0);
	glVertex3f(root_x	  , root_y + width , 0);
	glVertex3f(root_x + width , root_y + width , 0);
	glVertex3f(root_x + width , root_y	   , 0);
	glEnd();

	glFlush();
}

void reshape(int w, int h)
{
    /*if ( w != h)
    {
        WINDOWSIZE = max(w, h);
        glutReshapeWindow(WINDOWSIZE, WINDOWSIZE);
    	glOrtho(-1.0, 1.0, -1.0, 1.0, -1.0, 1.0);
    }*/
    WINDOWSIZE = min(w, h);
}

int startGame(int argc, char **argv)
{
	glutInit(&argc, argv);
	glEnable(GL_MULTISAMPLE);  
	glutSetOption(GLUT_MULTISAMPLE, 8);
	glutInitDisplayMode(GLUT_SINGLE | GLUT_MULTISAMPLE | GLUT_RGBA);
	glutInitWindowSize(WINDOWSIZE, WINDOWSIZE);
	glutInitWindowPosition(100, 100);
	genOcclusionMatrix();
	initGrid();
	
	if (is_server) {
		glutCreateWindow("NetChess [Server]");
	}
	else {
		glutCreateWindow("NetChess [Client]");
	}
	
	glutDisplayFunc(draw);
	//glutReshapeFunc(reshape);
	//glutPassiveMotionFunc(onMouseMove);
	glutMouseFunc(onMouseClick);
	glutTimerFunc(UPDATEDELAY, glutUpdateFunc, 0);
	
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_BLEND);
	
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
	texture = loadMemTexture();
	
	glutMainLoop();
	return 0;
}

/*void onMouseMove(int x, int y)
{
	mouseX = -1.0 + (float)2*x / WINDOWSIZE;
	mouseY = +1.0 - (float)2*y / WINDOWSIZE;
}*/

void onMouseClick(int button, int state, int pixelX, int pixelY)
{
	//check if it's our turn
	if (plyCount % 2 == is_server) { return; }

	float cellWidth = WINDOWSIZE / GRIDSIZE;
	if (button != GLUT_LEFT_BUTTON) {
		return;
	}

	struct boardSquare tile;
	tile.x = (char)(pixelX / cellWidth);
	tile.y = (char)(GRIDSIZE - pixelY/cellWidth);

	if (state == GLUT_DOWN) {
		cursor = tile;
	}
	else {
		//send move
		struct chessMove move;
		move.from = cursor;
		move.to = tile;
		if (isMoveValid(cursor, tile)) {
			if (movePiece(cursor,tile)) {
				sendMove(move);
				plyCount += 1;
			}
		}
		cursor.x = cursor.y = 0xff;
	}
}

void glutUpdateFunc(int value)
{
    update();
	glutPostRedisplay();
	glutTimerFunc(UPDATEDELAY, glutUpdateFunc, 0);
}

