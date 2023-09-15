import pygame
import random

# Initialize Pygame
pygame.init()

# Screen dimensions (phone-like resolution)
SCREEN_WIDTH = 430
SCREEN_HEIGHT = 950

# Colors
RED = (239, 64, 56)
GREEN = (144, 167, 112)
BLUE = (0, 0, 0)

# Circle settings
CIRCLE_RADIUS = 30
CIRCLE_SPEED = 3  # Adjust the speed as needed

# Function to generate random position for the circle
def random_position():
    return random.randint(CIRCLE_RADIUS, SCREEN_WIDTH - CIRCLE_RADIUS), random.randint(CIRCLE_RADIUS, SCREEN_HEIGHT - CIRCLE_RADIUS)

# Function to generate random velocity for the circle
def random_velocity():
    return random.uniform(-CIRCLE_SPEED, CIRCLE_SPEED), random.uniform(-CIRCLE_SPEED, CIRCLE_SPEED)

# Create the screen
screen = pygame.display.set_mode((SCREEN_WIDTH, SCREEN_HEIGHT))
pygame.display.set_caption("Moving Circles")

# List to store circles
circles = []

blue_balls = []

# Font settings for displaying FPS
font = pygame.font.Font(None, 36)

count = 0

# Game loop
running = True
while running:
    for event in pygame.event.get():
        if event.type == pygame.QUIT:
            running = False
        elif event.type == pygame.MOUSEBUTTONDOWN:
            # Check if a circle was clicked and remove it
            x, y = event.pos
            for circle in circles:
                pos, _, _ = circle
                circle_x, circle_y = pos
                if (x - circle_x) ** 2 + (y - circle_y) ** 2 < CIRCLE_RADIUS ** 2:
                    circles.remove(circle)
                    count += 1
                    print('[+] Count -', count)

            for circle in blue_balls:
                pos, _, _ , _= circle
                circle_x, circle_y = pos
                if (x - circle_x) ** 2 + (y - circle_y) ** 2 < CIRCLE_RADIUS ** 2:
                    blue_balls.remove(circle)
                    print('BLUE BALL CLICKED!')

    # Add a new circle with a random color and velocity every 60 frames
    if len(circles) < 5 and pygame.time.get_ticks() % 60 == 0:
        color = random.choice([RED, GREEN])
        pos = random_position()
        vel = random_velocity()
        circles.append((pos, color, vel))
    
    # Add a new blue ball with a random velocity every 80 frames
    if len(blue_balls) < 2 and pygame.time.get_ticks() % 80 == 0:
        pos = random_position()
        vel = random_velocity()
        blue_balls.append((pos, BLUE, vel, 0))  # The last value is bounce count


    # Clear the screen
    screen.fill((255, 255, 255))

    for idx, blue_ball in enumerate(blue_balls):
        pos, color, vel, bounce_count = blue_ball
        x, y = pos
        dx, dy = vel

        # Update blue ball position
        x += dx
        y += dy

        # Bounce off the screen edges
        if x <= CIRCLE_RADIUS or x >= SCREEN_WIDTH - CIRCLE_RADIUS:
            dx = -dx
            bounce_count += 1
        if y <= CIRCLE_RADIUS or y >= SCREEN_HEIGHT - CIRCLE_RADIUS:
            dy = -dy
            bounce_count += 1

        # Remove blue ball after 2 wall bounces
        if bounce_count >= 2:
            blue_balls.pop(idx)
            continue

        # Update the blue ball in the list
        blue_balls[idx] = ((x, y), color, (dx, dy), bounce_count)

        # Draw blue ball on the screen
        pygame.draw.circle(screen, color, (int(x), int(y)), CIRCLE_RADIUS)


    for idx, circle in enumerate(circles):
        pos, color, vel = circle
        x, y = pos
        dx, dy = vel

        # Update circle position
        x += dx
        y += dy

        # Bounce off the screen edges
        if x <= CIRCLE_RADIUS or x >= SCREEN_WIDTH - CIRCLE_RADIUS:
            dx = -dx
        if y <= CIRCLE_RADIUS or y >= SCREEN_HEIGHT - CIRCLE_RADIUS:
            dy = -dy

        # Update the circle in the list
        circles[idx] = ((x, y), color, (dx, dy))

        # Draw circle on the screen
        pygame.draw.circle(screen, color, (int(x), int(y)), CIRCLE_RADIUS)

    # Update the display
    pygame.display.flip()

# Quit Pygame
pygame.quit()
