import pygame
import random
import argparse
import time
from typing import List, Tuple
from pydantic import BaseModel, Field, validator
from csscolor import parse

# Initialize Pygame
pygame.init()

class GameConfig(BaseModel):
    screen_width: int = Field(..., gt=0)
    screen_height: int = Field(..., gt=0)
    num_circles: int = Field(..., gt=0)
    circle_speed: float = Field(..., gt=0)
    fps: int = Field(..., gt=0)
    duration: int = Field(..., gt=0)
    good_colors: List[str] = Field(..., min_items=1)
    bad_colors: List[str] = Field(..., min_items=1)
    good_circle_ratio: float = Field(0.7, ge=0, le=1)
    max_bumps: int = Field(3, gt=0)

    @validator('good_colors', 'bad_colors', each_item=True)
    def validate_color(cls, v):
        try:
            parse.color(v)
        except ValueError:
            raise ValueError(f"Invalid color format: {v}")
        return v

class Circle:
    def __init__(self, color: str, pos: Tuple[float, float], vel: Tuple[float, float], radius: int = 30, max_bumps: int = 3):
        self.color = self.parse_color(color)
        self.pos = list(pos)
        self.vel = list(vel)
        self.radius = radius
        self.bumps = 0
        self.max_bumps = max_bumps
    
    @staticmethod
    def parse_color(color: str) -> Tuple[int, int, int]:
        return parse.color(color).as_int_triple()

    def move(self, screen_width: int, screen_height: int):
        self.pos[0] += self.vel[0]
        self.pos[1] += self.vel[1]

        if self.pos[0] <= self.radius or self.pos[0] >= screen_width - self.radius:
            self.vel[0] = -self.vel[0]
            self.bumps += 1
        if self.pos[1] <= self.radius or self.pos[1] >= screen_height - self.radius:
            self.vel[1] = -self.vel[1]
            self.bumps += 1
    
    def draw(self, screen: pygame.Surface):
        pygame.draw.circle(screen, self.color, (int(self.pos[0]), int(self.pos[1])), self.radius)

    def clicked(self, mouse_pos: Tuple[int, int]) -> bool:
        dist_x = mouse_pos[0] - self.pos[0]
        dist_y = mouse_pos[1] - self.pos[1]
        distance = (dist_x ** 2 + dist_y ** 2) ** 0.5
        return distance < self.radius

    def should_disappear(self) -> bool:
        return self.bumps >= self.max_bumps

def random_position(screen_width: int, screen_height: int, radius: int) -> Tuple[int, int]:
    return (
        random.randint(radius, screen_width - radius), 
        random.randint(radius, screen_height - radius)
    )

def random_velocity(speed: float) -> Tuple[float, float]:
    return random.uniform(-speed, speed), random.uniform(-speed, speed)

def create_circle(config: GameConfig) -> Circle:
    if random.random() < config.good_circle_ratio:
        color = random.choice(config.good_colors)
    else:
        color = random.choice(config.bad_colors)
    pos = random_position(config.screen_width, config.screen_height, 30)
    vel = random_velocity(config.circle_speed)
    return Circle(color, pos, vel, max_bumps=config.max_bumps)

def run_game(config: GameConfig):
    screen = pygame.display.set_mode((config.screen_width, config.screen_height))
    pygame.display.set_caption("Circle Click Game")
    clock = pygame.time.Clock()

    circles: List[Circle] = [create_circle(config) for _ in range(config.num_circles)]

    good_clicks = 0
    bad_clicks = 0
    start_time = time.time()
    
    running = True
    while running and (time.time() - start_time) < config.duration:
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                running = False
            elif event.type == pygame.MOUSEBUTTONDOWN:
                mouse_pos = event.pos
                for circle in circles[:]:
                    if circle.clicked(mouse_pos):
                        if circle.color in [Circle.parse_color(c) for c in config.good_colors]:
                            good_clicks += 1
                        else:
                            bad_clicks += 1
                        circles.remove(circle)
                        circles.append(create_circle(config))

        screen.fill((0, 0, 0))  # Clear screen with black
        for circle in circles[:]:
            circle.move(config.screen_width, config.screen_height)
            if circle.should_disappear():
                circles.remove(circle)
                circles.append(create_circle(config))
            else:
                circle.draw(screen)

        pygame.display.flip()
        clock.tick(config.fps)

    pygame.quit()
    print(f"[+] Game Over!\n[+] Good clicks: {good_clicks}\n[+] Bad clicks: {bad_clicks}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Circle Click Game")
    parser.add_argument("--config", type=str, default="config.json", help="Path to the configuration file")
    args = parser.parse_args()

    config = GameConfig.parse_file(args.config)
    run_game(config)