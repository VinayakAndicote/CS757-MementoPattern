#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Box.H>
#include <iostream>
#include <FL/Fl_Button.H>
#include <FL/Fl_Input.H>
#include <cstdlib>
#include <ctime>
#include <sstream>
#include <iostream>
#include <vector>
#include <algorithm>



using namespace std;

#define WINDOW_WIDTH 1000
#define WINDOW_HEIGHT 900
#define PADDLE_WIDTH 100
#define PADDLE_HEIGHT 12
#define BALL_RADIUS 12
#define BRICK_WIDTH 50
#define BRICK_HEIGHT 15
#define NUM_BRICKS_X 3
#define NUM_BRICKS_Y 3

// memento
class ScoreState
{
private:
    int score;

public:
    ScoreState(int s) : score(s) {}

    int returnScore()
    {
        return score;
    }
};

// Caretaker
class GameHistory
{
private:
    vector<ScoreState> scores;

public:
    void push(ScoreState state)
    {
        scores.push_back(state);
    }
    ScoreState *lastScore()
    {
        if (!scores.empty())
        {
            ScoreState *lastScore = &scores.back();
            return lastScore;
        }
        else
            return new ScoreState(0);
    }

    string usersPastScores()
    {
        string result = "";
        for (ScoreState &x : scores)
        {
            result += to_string(x.returnScore()) + " ";
        }
        return result;
    }
};

class Ball
{
public:
    int x, y;
    int radius;
    int xSpeed, ySpeed;
    Fl_Box *box;

    Ball(int startX, int startY, int startRadius, int startXSpeed, int startYSpeed)
        : x(startX), y(startY), radius(startRadius), xSpeed(startXSpeed), ySpeed(startYSpeed),
          box(new Fl_Box(x - radius, y - radius, 2 * radius, 2 * radius))
    {
        box->box(FL_ROUND_UP_BOX);
        box->color(FL_RED);
    }

    void move()
    {
        x += xSpeed;
        y += ySpeed;
        box->position(x - radius, y - radius);
    }

    void resetBallSpeed()
    {
        xSpeed = 2;
        ySpeed = 2;
    }
};

class Paddle
{
public:
    int x, y;
    int width, height;
    Fl_Box *box;

    Paddle(int startX, int startY, int paddleWidth, int paddleHeight)
        : x(startX), y(startY), width(paddleWidth), height(paddleHeight),
          box(new Fl_Box(x, y, width, height))
    {
        box->box(FL_UP_BOX);
        box->color(FL_BLUE);
    }

    void move(int newX)
    {
        x = newX;
        box->position(x, y);
    }

    // whether ball collides with paddle
    bool collidesWithBall(Ball *ball)
    {
        return (ball->x > x && ball->x < x + width) && (ball->y > y && ball->y < y + height);
    }
};

class Brick
{
public:
    int x, y;
    int width, height;
    Fl_Box *box;

    Brick(int startX, int startY, int brickWidth, int brickHeight)
        : x(startX), y(startY), width(brickWidth), height(brickHeight),
          box(new Fl_Box(x, y, width, height))
    {
        box->box(FL_FLAT_BOX);
        box->color(FL_GREEN);
    }
    // check if ball collides with brick
    bool collidesWithBall(Ball *ball)
    {
        return (ball->x + ball->radius > x && ball->x - ball->radius < x + width) &&
               (ball->y + ball->radius > y && ball->y - ball->radius < y + height);
    }

    void move(int xpos, int ypos)
    {
        x = xpos;
        y = ypos;
        box->position(x, y);
    }
};

Brick *hiddenBricks[NUM_BRICKS_X][NUM_BRICKS_Y];

// Originator in memento pattern
class BallBricksGame : public Fl_Window
{
public:
    Ball *ball;
    Paddle *paddle;
    Brick *bricks[NUM_BRICKS_X][NUM_BRICKS_Y];
    int score;
    int playerLife = 2;
    bool isPaused;

    Fl_Box *scoreBox; 
    Fl_Box *playerLifeBox;
    Fl_Box *historyBox;
    Fl_Button *lifeLineButton;
    Fl_Button *quitButton;
    GameHistory *gameHistory;

    const double brickAddInterval;

    BallBricksGame(int width, int height, const char *title)
        : Fl_Window(width, height, title),
          ball(new Ball(width / 2, height / 2, BALL_RADIUS, 2, 2)),
          paddle(new Paddle(width / 2 - PADDLE_WIDTH / 2, height - PADDLE_HEIGHT - 10, PADDLE_WIDTH, PADDLE_HEIGHT)),
          score(0),
          brickAddInterval(2.0),
          isPaused(false),
          quitButton(new Fl_Button(width / 2 - 50, height / 2 + 30, 100, 40, "Quit")),
          lifeLineButton(new Fl_Button(width / 2 - 60, height / 2 + 90, 120, 40, "use Bonus life"))
    {
        // History to store Memento
        gameHistory = new GameHistory();

        quitButton->hide();
        lifeLineButton->hide();
        quitButton->callback(quitCallback, this);
        lifeLineButton->callback(lifeLineCallback, this);

        initializeBricks();
        scoreBox = new Fl_Box(10, 30, 100, 20); 
        scoreBox->align(FL_ALIGN_TOP_LEFT);
        scoreBox->copy_label("Score: 0"); // Initialize scoreBox

        // player life
        playerLifeBox = new Fl_Box(100, 30, 100, 20); 
        playerLifeBox->align(FL_ALIGN_TOP_LEFT);
        playerLifeBox->copy_label("PlayerLife: 2"); // Initialize playerLifeBox

        // player life
        historyBox = new Fl_Box(300, 30, 100, 20); 
        historyBox->align(FL_ALIGN_TOP_LEFT);
        historyBox->copy_label("PastScores:"); // Initialize historyBox

        // calls this 2 fn every 0.01 sec,i.e to get 100fps
        Fl::add_timeout(0.01, timerCallback, this);
        Fl::add_timeout(0.01, paddleMoveCallback, this);
        // adds bricks every 2 sec
        Fl::add_timeout(brickAddInterval, addRandomBrickCallback, this);
    }

    // memento patterns functionality
    void storeGameState()
    {
        // Creating Memento
        ScoreState *s = new ScoreState(score);
        gameHistory->push(*s);
    
    }
    void restartGameWithLifeLine()
    {
        ScoreState *s = gameHistory->lastScore();
        int bestScore = score;
        if (s->returnScore())
        {
            bestScore = max(s->returnScore(), bestScore);
        }
        resetGame(bestScore); // best score pass remaining
        playerLife--;

        updateScoreDisplay();
    }

    void initializeBricks()
    {
        srand(static_cast<unsigned>(time(nullptr))); // Seed the random number generator

        for (int i = 0; i < NUM_BRICKS_X; ++i)
        {
            for (int j = 0; j < NUM_BRICKS_Y; ++j)
            {
                int brickX = rand() % (WINDOW_WIDTH - BRICK_WIDTH); 
                int brickY = rand() % (WINDOW_HEIGHT / 2);          

                bricks[i][j] = new Brick(brickX, brickY, BRICK_WIDTH, BRICK_HEIGHT);

                bricks[i][j]->box->color(FL_GREEN);
            }
        }
       
    }

    void resetGame(int newscore = 0)
    {
        storeGameState();
        score = newscore;
        ball->x = WINDOW_WIDTH / 2;
        ball->y = WINDOW_HEIGHT / 2;

        ball->resetBallSpeed();

        paddle->move(WINDOW_WIDTH / 2 - PADDLE_WIDTH / 2);

        updateScoreDisplay();

        quitButton->hide();
        lifeLineButton->hide();
        resumeGame();
    }



// resume game when users users its life
    void resumeGame()
    {
        isPaused = false;
        Fl::add_timeout(0.01, paddleMoveCallback, this);                
        Fl::add_timeout(brickAddInterval, addRandomBrickCallback, this); 
    }

    static void quitCallback(Fl_Widget *widget, void *userData)
    {
        BallBricksGame *game = static_cast<BallBricksGame *>(userData);
        game->storeGameState();
        game->gameHistory->usersPastScores();
        game->hide(); 
    }

    static void lifeLineCallback(Fl_Widget *widget, void *userData)
    {
        BallBricksGame *game = static_cast<BallBricksGame *>(userData);
        game->restartGameWithLifeLine();
    }

    void draw() override
    {
        Fl_Window::draw();
        ball->move();

        // Render the paddle
        paddle->box->redraw();

        handleCollisions();
    }

    static void timerCallback(void *userData)
    {
        BallBricksGame *game = static_cast<BallBricksGame *>(userData);
        game->redraw();
        Fl::repeat_timeout(0.01, timerCallback, userData);
    }

    static void paddleMoveCallback(void *userData)
    {
        BallBricksGame *game = static_cast<BallBricksGame *>(userData);
        int mouseX = Fl::event_x();
        game->paddle->move(mouseX);
        Fl::repeat_timeout(0.01, paddleMoveCallback, userData);
    }

    void handleCollisions()
    {
        // when paddle collides with ball
        if (paddle->collidesWithBall(ball))
        {
            ball->ySpeed = -(ball->ySpeed + 1);
        }

        for (int i = 0; i < NUM_BRICKS_X; ++i)
        {
            for (int j = 0; j < NUM_BRICKS_Y; ++j)
            {
                // when bricks collides with ball
                if (bricks[i][j] != nullptr && bricks[i][j]->collidesWithBall(ball))
                {
                    bricks[i][j]->box->color(FL_BLUE);

                    clearBricks(i, j); //destroy the brick
                    // Change direction of the ball;
                    ball->ySpeed = -(ball->ySpeed); 

                    score++;
                    updateScoreDisplay(); // Update the score display
                }
            }
        }

        // Ball missed, game over
        if (ball->y - ball->radius >= WINDOW_HEIGHT)
        {

            isPaused = true;
            quitButton->show();

            if (playerLife > 0)
                lifeLineButton->show();

            Fl::remove_timeout(paddleMoveCallback, this);     // Stop the paddle move callback
            Fl::remove_timeout(addRandomBrickCallback, this); //
        }

        // Bounce off walls
        if (ball->x - ball->radius <= 0 || ball->x + ball->radius >= WINDOW_WIDTH)
        {
            ball->xSpeed = -ball->xSpeed;
        }

        if (ball->y - ball->radius <= 0)
        {
            ball->ySpeed = -ball->ySpeed;
        }
    }

    //destroy the brick when ball collides with it
    void clearBricks(int i, int j)
    {

        if (bricks[i][j] != nullptr)
        {
            if (bricks[i][j]->box != nullptr)
            {
                hiddenBricks[i][j] = bricks[i][j];
                bricks[i][j]->box->hide(); 
                bricks[i][j] = nullptr;
            }
        }
    }

// update scores of user 
    void updateScoreDisplay()
    {
        char scoreText[20];
        snprintf(scoreText, sizeof(scoreText), "Score: %d", score);
        scoreBox->copy_label(scoreText); // Update the label of the scoreBox

        char lifeText[20];
        snprintf(lifeText, sizeof(scoreText), "PlayerLife: %d", playerLife);
        playerLifeBox->copy_label(lifeText);
        updatePastScores();
    }

    void updatePastScores()
    {

        char pastScore[40];
        snprintf(pastScore, sizeof(pastScore), "PastScores:: %s", gameHistory->usersPastScores().c_str());
        historyBox->copy_label(pastScore);
    }


    static void addRandomBrickCallback(void *userData)
    {
        BallBricksGame *game = static_cast<BallBricksGame *>(userData);
        game->addRandomBrick();

        // Reschedule the timeout for the next brick addition
        Fl::add_timeout(game->brickAddInterval, addRandomBrickCallback, userData);
        // cout << "will add new bricks soon" << endl;
    }

    void addRandomBrick()
    {
        int brickX = rand() % (WINDOW_WIDTH - BRICK_WIDTH);
        int brickY = rand() % (WINDOW_HEIGHT / 2);

        // Place new brick at empty slot
        for (int i = 0; i < NUM_BRICKS_X; ++i)
        {
            for (int j = 0; j < NUM_BRICKS_Y; ++j)
            {
                if (bricks[i][j] == nullptr)
                {
                    // cout << "Found a spot adding new brick" << endl;
                    bricks[i][j] = hiddenBricks[i][j];
                    bricks[i][j]->move(brickX, brickY);
                    bricks[i][j]->box->show();
                    bricks[i][j]->box->color(FL_YELLOW);

                    return;
                }
            }
        }
    }
};

int main()
{
    BallBricksGame game(WINDOW_WIDTH, WINDOW_HEIGHT, "Ball and Bricks Game");
    game.show();
    return Fl::run();
}