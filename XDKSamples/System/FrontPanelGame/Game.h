//--------------------------------------------------------------------------------------
// Game.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

#include "FrontPanel\FrontPanelDisplay.h"

enum DotState
{
    Empty,
    Food,
    Filled,
};

struct Dot
{
    Dot(int x, int y)
        : State(DotState::Empty)
        , X(x)
        , Y(y)
    {};

    DotState State;
    int X;
    int Y;
};

class GameBoard
{
public:

    GameBoard(ATG::FrontPanelDisplay &display);

    void Render() const;

    void SpawnFood();

    void SetDotState(std::shared_ptr<Dot> dot, DotState state);

    std::shared_ptr<Dot> GetDot(int x, int y);

private:

    static const int GAME_BOARD_LEFT = 256 / 4 + 2;
    static const int GAME_BOARD_TOP = 2;

    static const int GAME_BOARD_DOT_SIZE = 4;

    static const int GAME_BOARD_WIDTH = (254 - GAME_BOARD_LEFT) / GAME_BOARD_DOT_SIZE;
    static const int GAME_BOARD_HEIGHT = (62 - GAME_BOARD_TOP) / GAME_BOARD_DOT_SIZE;

    static const int GAME_BOARD_OUTLINE_LEFT = GAME_BOARD_LEFT - 1;
    static const int GAME_BOARD_OUTLINE_TOP = GAME_BOARD_TOP - 1;

    static const int GAME_BOARD_OUTLINE_WIDTH = GAME_BOARD_WIDTH * GAME_BOARD_DOT_SIZE + 2;
    static const int GAME_BOARD_OUTLINE_HEIGHT = GAME_BOARD_HEIGHT *  GAME_BOARD_DOT_SIZE + 2;

    ATG::FrontPanelDisplay &frontPanelDisplay;

    std::vector<std::shared_ptr<Dot>> blankDots;

    std::shared_ptr<Dot> gameBoard[GAME_BOARD_WIDTH][GAME_BOARD_HEIGHT];
};

enum SnakeMoveResult
{
    Move,
    Fail,
    Eat,
};

class Snake
{
public:

    Snake(
        std::shared_ptr<GameBoard> board,
        int directionX,
        int directionY,
        int startX,
        int startY);

    void SetDirection(int x, int y);

    SnakeMoveResult Move();

private:

    std::shared_ptr<GameBoard> gameBoard;

    std::list<std::shared_ptr<Dot>> Body;
    int DirectionX;
    int DirectionY;

    int BackwardsDirectionX;
    int BackwardsDirectionY;
};

