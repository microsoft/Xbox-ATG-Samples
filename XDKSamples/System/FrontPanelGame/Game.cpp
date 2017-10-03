//--------------------------------------------------------------------------------------
// Game.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "pch.h"
#include "Game.h"

#include "FrontPanel\CPUShapes.h"

using namespace ATG;

namespace
{
    int RandInt(int low, int high)
    {
        return (rand() % (high - low + 1)) + low;
    }
} // ANONYMOUS namespace

GameBoard::GameBoard(FrontPanelDisplay &display)
    : frontPanelDisplay(display)
{
    blankDots.reserve(GAME_BOARD_WIDTH * GAME_BOARD_HEIGHT);

    for (int x = 0; x < GAME_BOARD_WIDTH; x++)
    {
        for (int y = 0; y < GAME_BOARD_HEIGHT; y++)
        {
            auto newDot = std::make_shared<Dot>(x, y);

            gameBoard[x][y] = newDot;

            blankDots.push_back(newDot);
        }
    }
}

void GameBoard::Render() const
{

    CPUShapes shapes = CPUShapes(frontPanelDisplay.GetDisplayWidth(), frontPanelDisplay.GetDisplayHeight(), frontPanelDisplay.GetBuffer());
    for (int x = 0; x < GAME_BOARD_WIDTH; x++)
    {
        for (int y = 0; y < GAME_BOARD_HEIGHT; y++)
        {
            if (gameBoard[x][y]->State != DotState::Empty)
            {
                shapes.RenderRect(
                    GAME_BOARD_LEFT + GAME_BOARD_DOT_SIZE * x,
                    GAME_BOARD_TOP + GAME_BOARD_DOT_SIZE * y,
                    GAME_BOARD_DOT_SIZE, GAME_BOARD_DOT_SIZE);
            }
        }
    }

    shapes.RenderRect(
        GAME_BOARD_OUTLINE_LEFT,
        GAME_BOARD_OUTLINE_TOP,
        GAME_BOARD_OUTLINE_WIDTH,
        GAME_BOARD_OUTLINE_HEIGHT,
        0x77,
        false);
}

void GameBoard::SpawnFood()
{
    if (blankDots.size() > 0)
    {
        int index = RandInt(0, (int)blankDots.size() - 1);

        SetDotState(blankDots[index], DotState::Food);
    }
}

void GameBoard::SetDotState(std::shared_ptr<Dot> dot, DotState state)
{
    if (state == dot->State)
    {
        return;
    }
    else if (dot->State == DotState::Empty && state != DotState::Empty)
    {
        for (unsigned int i = 0; i < blankDots.size(); i++)
        {
            if (dot == blankDots[i])
            {
                blankDots[i] = blankDots[blankDots.size() - 1];

                blankDots.pop_back();

                break;
            }
        }
    }
    else if (dot->State != DotState::Empty && state == DotState::Empty)
    {
        blankDots.push_back(dot);
    }

    dot->State = state;
}

std::shared_ptr<Dot> GameBoard::GetDot(int x, int y)
{
    if (x < 0 || x >= GAME_BOARD_WIDTH
        || y < 0 || y >= GAME_BOARD_HEIGHT)
    {
        return nullptr;
    }
    else
    {
        return gameBoard[x][y];
    }
}

Snake::Snake(
    std::shared_ptr<GameBoard> board,
    int directionX,
    int directionY,
    int startX,
    int startY)
    : DirectionX(0)
    , DirectionY(0)
    , BackwardsDirectionX(0)
    , BackwardsDirectionY(0)
{
    gameBoard = board;

    DirectionX = directionX;
    DirectionY = directionY;

    BackwardsDirectionX = -DirectionX;
    BackwardsDirectionY = -DirectionY;

    Body.push_front(gameBoard->GetDot(startX, startY));
}

void Snake::SetDirection(int x, int y)
{
    if (BackwardsDirectionX != x || BackwardsDirectionY != y)
    {
        DirectionX = x;
        DirectionY = y;
    }
}

SnakeMoveResult Snake::Move()
{
    auto head = Body.front();

    auto next = gameBoard->GetDot(head->X + DirectionX, head->Y + DirectionY);

    BackwardsDirectionX = -DirectionX;
    BackwardsDirectionY = -DirectionY;

    if (next == nullptr)
    {
        return SnakeMoveResult::Fail;
    }

    switch (next->State)
    {
    case DotState::Empty:

        gameBoard->SetDotState(next, DotState::Filled);

        Body.push_front(next);

        gameBoard->SetDotState(Body.back(), DotState::Empty);

        Body.pop_back();

        return SnakeMoveResult::Move;

    case DotState::Food:

        gameBoard->SetDotState(next, DotState::Filled);

        Body.push_front(next);

        return SnakeMoveResult::Eat;

    case DotState::Filled:
    default:

        return SnakeMoveResult::Fail;
    }
}
