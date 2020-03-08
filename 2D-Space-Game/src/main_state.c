#include <main_state.h>
#include <glad/glad.h>
#include <math.h>
#include <time.h>

#include <rafgl.h>

#include <game_constants.h>

static rafgl_raster_t doge;
static rafgl_raster_t upscaled_doge;
static rafgl_raster_t raster, raster2;
static rafgl_raster_t checker;
static rafgl_raster_t starship;
static rafgl_raster_t end;
static rafgl_spritesheet_t ship_fire;
static rafgl_raster_t perlin;

static rafgl_texture_t texture;

#define TILE_SIZE 64
#define MAX_STARS 100
#define MAX_PROJECTILES 3
#define MAX_ENEMIES 1
#define MAX_PARTICLES 20

static int raster_width = RASTER_WIDTH, raster_height = RASTER_HEIGHT;

static char save_file[256];
int save_file_no = 0;

typedef struct _particle_t{
    float x,y,dx,dy;
    int life;
} particle_t;

typedef struct _enemy_t{
    int x,y,radius;
    int health;
} enemy_t;

typedef struct _star_t{
    int x,y,radius;
    int brightness;
} star_t;

typedef struct _projectile_t{
    int x,y,dy;
    int power, life;
} projectile_t;

particle_t particles[MAX_PARTICLES];
enemy_t enemies[MAX_ENEMIES];
projectile_t projectiles[MAX_PROJECTILES];
star_t stars[MAX_STARS];
int fadeFrames = 0;
int phase = 0;
int glitch_duration = 0;
int death_duration = 0;
int allow_to_move = 0;
int end_y = 0;
int end_y2 = 800;
int fire_animation = 0;
int fire_speed = 0;
int deltaBrightness = 15;

/*---------------PERLIN---------------*/
float cosine_interpolationf(float a, float b, float s)
{
    float f = (1.0f - cosf(s * M_PI))* 0.5f;
    return a + (b - a) * f;
}

void cosine_float_map_rescale(float *dst, int dst_width, int dst_height, float *src, int src_width, int src_height)
{
    int x, y;
    float xn, yn;
    float fxs, fys;
    int ixs0, iys0;
    int ixs1, iys1;
    float upper_middle, lower_middle;
    float sample_left, sample_right;
    float result;

    for(y = 0; y < dst_height; y++)
    {
        yn = 1.0f * y / dst_height;
        fys = yn * src_height;
        iys0 = fys;
        iys1 = iys0 + 1;
        fys -= iys0;
        if(iys1 >= src_height) iys1 = src_height - 1;
        for(x = 0; x < dst_width; x++)
        {
            xn = 1.0f * x / dst_width;
            fxs = xn * src_width;
            ixs0 = fxs;
            ixs1 = ixs0 + 1;
            if(ixs1 >= src_width) ixs1 = src_width - 1;
            fxs -= ixs0;

            sample_left = src[iys0 * src_width + ixs0];
            sample_right = src[iys0 * src_width + ixs1];
            upper_middle = cosine_interpolationf(sample_left, sample_right, fxs);

            sample_left = src[iys1 * src_width + ixs0];
            sample_right = src[iys1 * src_width + ixs1];
            lower_middle = cosine_interpolationf(sample_left, sample_right, fxs);

            result = cosine_interpolationf(upper_middle, lower_middle, fys);

            dst[y * dst_width + x] = result;


        }
    }
}

void float_map_multiply_and_add(float *dst, float *src, int w, int h, float multiplier)
{
    int x, y;
    for(y = 0; y < h; y++)
    {
        for(x = 0; x < w; x++)
        {
            dst[y * w + x] += src[y * w + x] * multiplier;
        }
    }
}


rafgl_raster_t generate_perlin_noise(int octaves, float persistance)
{
    int octave_size = 2;
    float multiplier = 1.0f;

    rafgl_raster_t r;

    int width = pow(2, octaves);
    int height = width;

    int i, octave, x, y;
    float *tmp_map = malloc(height * width * sizeof(float));
    float *final_map = calloc(height * width, sizeof(float));
    float *octave_map;
    rafgl_pixel_rgb_t pix;
    rafgl_raster_init(&r, width, height);


    for(octave = 0; octave < octaves; octave++)
    {
        octave_map = malloc(octave_size * octave_size * sizeof(float));
        for(y = 0; y < octave_size; y++)
        {
            for(x = 0; x < octave_size; x++)
            {
                octave_map[y * octave_size + x] = randf() * 2.0f - 1.0f;
            }
        }

        cosine_float_map_rescale(tmp_map, width, height, octave_map, octave_size, octave_size);
        float_map_multiply_and_add(final_map, tmp_map, width, height, multiplier);

        octave_size *= 2;
        multiplier *= persistance;

        free(octave_map);
    }

    float sample;
    for(y = 0; y < height; y++)
    {
        for(x = 0; x < width; x++)
        {
            sample = final_map[y * width + x];
            sample = (sample + 1.0f) / 2.0f;
            if(sample < 0.0f) sample = 0.0f;
            if(sample > 1.0f) sample = 1.0f;
            pix.r = sample * 255;
            pix.g = sample * 255;
            pix.b = sample * 255;

            pixel_at_m(r, x, y) = pix;
        }
    }


    free(final_map);
    free(tmp_map);

    return r;
}


/*---------------PERLIN---------------*/

void drawParticles(rafgl_raster_t *raster)
{
    int i;
    particle_t p;
    for(i = 0; i < MAX_PARTICLES; i++){
        p = particles[i];
        if(p.life <= 0) continue;
        rafgl_raster_draw_line(raster, p.x - p.dx, p.y - p.dy, p.x, p.y, rafgl_RGB(255, 0, 0));
    }
}

void updateParticles(){
    int i;
    for(i = 0; i < MAX_PARTICLES; i++){
        if(particles[i].life <= 0) continue;

        particles[i].life--;

        particles[i].x += particles[i].dx;
        particles[i].y += particles[i].dy;
        particles[i].dx *= 0.995f;
        particles[i].dy *= 0.995f;
        particles[i].dy += 0.05;

        if(particles[i].x < 0 || particles[i].x > raster_width || particles[i].y < 0 || particles[i].y > raster_height){
            particles[i].life = 0;
        }

    }
}

void initEnemies(){
   int i;
   for(i=0;i<MAX_ENEMIES;i++){
        time_t seconds;
        seconds = time(NULL);
        enemies[i].x = rand() * seconds % (raster_width - 50) + 50;
        enemies[i].y = rand() % (raster_height / (i*2 + 1)) + 100;
        enemies[i].radius = 15;
        enemies[i].health = 3;
   }
}

void updateEnemies(){
    int i;
    int end = 1;
    for(i=0;i<MAX_ENEMIES;i++){
        if(enemies[i].radius>0){
            enemies[i].x = (enemies[i].x + 2) % raster_width;
            /*
            enemies[i].y += (sin(phase * (3.14/180))*10)/4;
            phase = ++phase % 360;
            */
            end = 0;
        }
    }
    if(end) endAnimation();
}

void drawEnemies(rafgl_raster_t *raster){
    int i,j;
    for(i=0;i<MAX_ENEMIES;i++){
        if(enemies[i].health){
            rafgl_raster_draw_circle(raster,enemies[i].x,enemies[i].y,enemies[i].radius * enemies[i].health,rafgl_RGB(255,0,0));
        }else if(enemies[i].radius > 0){
            if(death_duration == 0){
                enemies[i].radius--;
                death_duration = 5;
            }else{
                death_duration--;
            }
            rafgl_raster_draw_circle(raster,enemies[i].x,enemies[i].y,enemies[i].radius,rafgl_RGB(255,0,0));
        }else if(enemies[i].radius == 0){
            float angle, speed;
            for(j = 0; j < MAX_PARTICLES; j++){
                if(particles[j].life <= 0){
                    particles[j].life = 100 * randf() + 100;
                    particles[j].x = enemies[i].x;
                    particles[j].y = enemies[i].y;
                    angle = randf() * M_PI *  2.0f;
                    speed = ( 0.3f + 0.7 * randf()) * 10;
                    particles[j].dx = cosf(angle) * speed;
                    particles[j].dy = sinf(angle) * speed;
                }
            }
            enemies[i].radius--;
        }
    }
}

void initProjectiles(){
    int i;
    for(i=0;i<MAX_PROJECTILES;i++){
        projectiles[i].life = 0;
    }
}

void initStars(){
    int i;
    for(i=0;i<MAX_STARS;i++){
      stars[i].x = rand() % (RASTER_WIDTH - 10) + 10;
      stars[i].y = rand() % (RASTER_HEIGHT - 10) + 10;
      stars[i].radius = 1;
      stars[i].brightness = rand() % 256 + 1;
    }
}

void drawStars(rafgl_raster_t *raster){
    int i;
    star_t star;
    for(i=0;i<MAX_STARS;i++){
        star = stars[i];
        rafgl_raster_draw_circle(raster,star.x,star.y,star.radius,rafgl_RGB(star.brightness,star.brightness,star.brightness));
        if(star.brightness>220){
            rafgl_raster_draw_line(raster,star.x-2,star.y,star.x+2,star.y,rafgl_RGB(star.brightness,star.brightness,star.brightness));
            rafgl_raster_draw_line(raster,star.x,star.y-2,star.x,star.y+2,rafgl_RGB(star.brightness,star.brightness,star.brightness));
        }
    }
}

void updateStars(){
    int i;
    if(fadeFrames == 0){
        for(i=0;i<MAX_STARS;i++){
            stars[i].brightness = rand() % 256 +1;
        }
        fadeFrames = 30;
    }else{
        fadeFrames--;
    }
}

int pressed;
float location = 0;
float selector = 0;

//moje

//starship
int starship_pos_x = RASTER_WIDTH / 2.2;
int starship_pos_y = RASTER_HEIGHT / 1.2;
int starship_speed = 150;
int color_r[TILE_SIZE][TILE_SIZE];
int color_g[TILE_SIZE][TILE_SIZE];
int color_b[TILE_SIZE][TILE_SIZE];
//starship

//input
int projectile_pressed;
int laser_pressed;
//input

//laser destination
int destination_x, destination_y;


void drawProjectile(rafgl_raster_t *raster){
    int i;
    for(i=0;i<MAX_PROJECTILES;i++){
        if(projectiles[i].life){
            rafgl_raster_draw_line(raster,projectiles[i].x, projectiles[i].y - projectiles[i].dy, projectiles[i].x, projectiles[i].y, rafgl_RGB(255,0,0));
        }
    }
}


void updateProjectile(){
    int i;
    for(i=0;i<MAX_PROJECTILES;i++){
        projectiles[i].y -= projectiles[i].dy;
        projectiles[i].dy /= 0.995f;
        if(projectiles[i].y < 0){
            projectiles[i].life = 0;
        }
    }
}

void hitDetection(){
    int i,j;
    for(i=0;i<MAX_PROJECTILES;i++){
        if(projectiles[i].life == 0) continue;
        for(j=0;j<MAX_ENEMIES;j++){
            if(enemies[j].health == 0) continue;
            double distance = rafgl_distance2D(projectiles[i].x,projectiles[i].y,enemies[j].x,enemies[j].y);
            if(distance <= enemies[j].radius * enemies[j].health){
                projectiles[i].life = 0;
                enemies[j].health--;
            }
        }
    }
}

void getColors(){
    int x,y;
    for(y=0;y<TILE_SIZE;y++){
        for(x=0;x<TILE_SIZE;x++){
            if(pixel_at_m(starship,x,y).rgba == rafgl_RGB(255,0,254)) continue;
            color_r[x][y] = pixel_at_m(starship,x,y).r;
            color_g[x][y] = pixel_at_m(starship,x,y).g;
            color_b[x][y] = pixel_at_m(starship,x,y).b;
        }
    }
}

void endAnimation(){
    allow_to_move = 0;
    starship_pos_y-=3;
    if(starship_pos_y + 64 < 0){
        for(int y=0;y<end_y;y++)
            for(int x=0;x<raster_width;x++)
                pixel_at_m(raster,x,y).rgba = rafgl_RGB(255,255,255);
        if(end_y<300)
            end_y++;
        for(int y=raster_height;y>end_y2;y--)
            for(int x=0;x<raster_width;x++)
                pixel_at_m(raster,x,y).rgba = rafgl_RGB(255,255,255);
        if(end_y2>500)
            end_y2--;
    }
    if(end_y == 300 && end_y2 == 500){
        //todo perlin noise text generate
        //rafgl_raster_draw_raster(&raster,&end,0,end_y);
        drawEndImage(&perlin,&end,0,end_y);
    }
}

int pixelDifference(rafgl_pixel_rgb_t first,rafgl_pixel_rgb_t second){
    return abs(rafgl_calculate_pixel_brightness(first)-rafgl_calculate_pixel_brightness(second));
}

void drawEndImage(rafgl_raster_t *effect, rafgl_raster_t *image, int x, int y){
    int i,j;
    for(j=0;j<image->height;j++){
        for(i=0;i<image->width;i++){
            if(pixel_at_pm(image,i,j).rgba == rafgl_RGB(255,0,254)) continue;
            if(pixelDifference(pixel_at_pm(effect,i,j),pixel_at_pm(image,i,j)) <= deltaBrightness){
                pixel_at_m(raster,i,j) = pixel_at_pm(image,i,j);
            }
        }
    }
    deltaBrightness++;
}


void main_state_init(GLFWwindow *window, void *args)
{
    /* inicijalizacija */
    /* raster init nam nije potreban ako radimo load from image */
    rafgl_raster_load_from_image(&doge, "res/images/doge.png");
    rafgl_raster_load_from_image(&checker, "res/images/checker32.png");

    //init starship image
    rafgl_raster_load_from_image(&starship, "res/tiles/starship.png");
    rafgl_raster_draw_raster(&raster,&starship,starship_pos_x,starship_pos_y);
    getColors();

    //init end image
    rafgl_raster_load_from_image(&end,"res/images/end1.png");

    //ship fire
    rafgl_spritesheet_init(&ship_fire, "res/images/fire.png", 4, 1);

    rafgl_raster_init(&upscaled_doge, raster_width, raster_height);
    rafgl_raster_bilinear_upsample(&upscaled_doge, &doge);


    rafgl_raster_init(&raster, raster_width, raster_height);
    rafgl_raster_init(&raster2, raster_width, raster_height);

    initStars();
    initProjectiles();
    initEnemies();
    perlin = generate_perlin_noise(10, 0.7f);
    rafgl_texture_init(&texture);
}


void main_state_update(GLFWwindow *window, float delta_time, rafgl_game_data_t *game_data, void *args)
{
    /* hendluj input */
    if(game_data->is_lmb_down && game_data->is_rmb_down)
    {
        pressed = 1;
        location = rafgl_clampf(game_data->mouse_pos_y, 0, raster_height - 1);
        selector = 1.0f * location / raster_height;
    }
    else
    {
        pressed = 0;
    }

    /*Moj deo*/
    if(game_data->keys_down[RAFGL_KEY_W] && allow_to_move){
        //Gore
        starship_pos_y = starship_pos_y - starship_speed * delta_time;
        if(starship_pos_y < 550) starship_pos_y = 550;
    }if(game_data->keys_down[RAFGL_KEY_S] && allow_to_move){
        //Dole
        starship_pos_y = starship_pos_y + starship_speed * delta_time;
        if(starship_pos_y + TILE_SIZE> RASTER_HEIGHT) starship_pos_y = RASTER_HEIGHT - TILE_SIZE;
    }if(game_data->keys_down[RAFGL_KEY_A] && allow_to_move){
        //Levo
        starship_pos_x = starship_pos_x - starship_speed * delta_time;
        if(starship_pos_x < 0) starship_pos_x = 0;
    }if(game_data->keys_down[RAFGL_KEY_D] && allow_to_move){
        //Desno
        starship_pos_x = starship_pos_x + starship_speed * delta_time;
        if(starship_pos_x > RASTER_WIDTH - TILE_SIZE) starship_pos_x = RASTER_WIDTH - TILE_SIZE;
    }

    //laser input
    if(game_data->is_lmb_down){
        destination_x = rafgl_clampf(game_data->mouse_pos_x, 0, raster_height - 1);
        destination_y = rafgl_clampf(game_data->mouse_pos_y, 0, raster_height - 1);
        laser_pressed = 1;
    }else{
        laser_pressed = 0;
    }

    //projectile input
    if(game_data->keys_down[RAFGL_KEY_SPACE] && projectile_pressed == 0){
        int i;
        for(i=0;i<MAX_PROJECTILES;i++){
            if(projectiles[i].life == 0){
                projectile_pressed = 1;
                projectiles[i].life = 1;
                projectiles[i].x = starship_pos_x + 32;
                projectiles[i].y = starship_pos_y + 32;
                projectiles[i].dy = 10;
                break;
            }
        }
    }else if(game_data->keys_down[RAFGL_KEY_SPACE] && projectile_pressed == 1){
        //Skip
    }else {
        projectile_pressed = 0;
    }


    /* izmeni raster */

    int x, y;

    float xn, yn;

    rafgl_pixel_rgb_t sampled, sampled2, resulting, resulting2;


    for(y = 0; y < raster_height; y++)
    {
        yn = 1.0f * y / raster_height;
        for(x = 0; x < raster_width; x++)
        {
            xn = 1.0f * x / raster_width;

            sampled = pixel_at_m(upscaled_doge, x, y);
            sampled2 = rafgl_point_sample(&doge, xn, yn);

            resulting = sampled;
            resulting2 = sampled2;

            resulting.rgba = rafgl_RGB(0, 0, 0);

            pixel_at_m(raster, x, y) = resulting;
            pixel_at_m(raster2, x, y) = resulting2;


            if(pressed && rafgl_distance1D(location, y) < 3 && x > raster_width - 15)
            {
                pixel_at_m(raster, x, y).rgba = rafgl_RGB(255, 0, 0);
            }
        }
    }

    //Crtanje zvezda
    updateStars(&fadeFrames);
    drawStars(&raster);

    //Crtanje broda
    int i = 0;
    int j = 0;
    if(glitch_duration == 200){
        if(fire_speed == 0){
            fire_animation = (fire_animation + 1) % 4;
            fire_speed = 5;
        }else{
            fire_speed--;
        }
        allow_to_move = 1;
        rafgl_raster_draw_spritesheet(&raster, &ship_fire, fire_animation, 0, starship_pos_x, starship_pos_y+56);
        rafgl_raster_draw_raster(&raster,&starship,starship_pos_x,starship_pos_y);
    }else{
        if(glitch_duration%67>=0 && glitch_duration%67<=2){
            rafgl_raster_draw_raster(&raster,&starship,starship_pos_x,starship_pos_y);
        }else{
            for(int y = starship_pos_y; y < TILE_SIZE + starship_pos_y; y++){
                i = 0;
                for(int x = starship_pos_x; x < TILE_SIZE + starship_pos_x; x++){
                    /* deformisemo koordinate */
                    int xnew = x + sinf(y * 0.9f) * 5;
                    int ynew = y + sinf(x * 0.9f) * 5;
                    //crtamo
                    pixel_at_m(raster, xnew - TILE_SIZE/4, y).b = color_b[i][j];
                    pixel_at_m(raster, xnew, ynew).r = color_r[i][j];
                    pixel_at_m(raster, xnew + TILE_SIZE/4, y).g = color_g[i][j];
                    i++;
                }
                j++;
            }
        }
        glitch_duration++;
    }


    //crtanje lasera
    if(laser_pressed){
        int alive = 0;
        int i;
        for(i=0;i<MAX_PROJECTILES;i++){
            if(projectiles[i].life == 1) alive++;
        }
        if(alive == 3){
            //Crtanje lasera
            rafgl_raster_draw_line(&raster,starship_pos_x + 32,starship_pos_y + 32,destination_x,destination_y,rafgl_RGB(255,0,0));
            int i;
            //Hit detection
            for(i=0;i<MAX_ENEMIES;i++){
                if(enemies[i].health){
                    double distance = rafgl_distance2D(destination_x,destination_y,enemies[i].x,enemies[i].y);
                    if(distance <= enemies[i].radius * enemies[i].health){
                        enemies[i].health = 0;
                    }
                }
            }
        }
    }

    //crtanje projektila
    updateProjectile(delta_time);
    hitDetection();
    drawProjectile(&raster);

    //crtanje neprijatelja
    updateEnemies();
    drawEnemies(&raster);

    //crtanje eksplozije
    updateParticles();
    drawParticles(&raster);



    /* shift + s snima raster */
    if(game_data->keys_pressed[RAFGL_KEY_S] && game_data->keys_down[RAFGL_KEY_LEFT_SHIFT])
    {
        sprintf(save_file, "save%d.png", save_file_no++);
        rafgl_raster_save_to_png(&raster, save_file);
    }

    rafgl_texture_load_from_raster(&texture, &raster);
}


void main_state_render(GLFWwindow *window, void *args)
{
    /* prikazi teksturu */
    rafgl_texture_show(&texture);
}


void main_state_cleanup(GLFWwindow *window, void *args)
{
    rafgl_raster_cleanup(&raster);
    rafgl_raster_cleanup(&raster2);
    rafgl_texture_cleanup(&texture);

}
