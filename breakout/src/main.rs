#[cfg(debug_assertions)]
use avian2d::debug_render::PhysicsDebugPlugin;
use avian2d::{
    PhysicsPlugins,
    collision::collider::Collider,
    dynamics::rigid_body::{
        CoefficientCombine, GravityScale, LinearVelocity, LockedAxes, Restitution, RigidBody,
    },
    math::AdjustPrecision,
    physics_transform::Position,
};
use bevy::{
    DefaultPlugins,
    app::{App, AppExit, FixedUpdate, PluginGroup, Startup},
    asset::Assets,
    camera::Camera2d,
    color::Color,
    ecs::{
        component::Component,
        query::{With, Without},
        schedule::IntoScheduleConfigs,
        system::{Commands, Res, ResMut, Single},
    },
    input::{ButtonInput, keyboard::KeyCode},
    log,
    math::{
        Vec2,
        primitives::{Circle, Rectangle},
    },
    mesh::{Mesh, Mesh2d},
    sprite_render::{ColorMaterial, MeshMaterial2d},
    window::{Window, WindowPlugin},
};

const SCREEN_WIDTH: u32 = 800;
const SCREEN_HEIGHT: u32 = 450;

const PADDLE_SPEED: f32 = 200.0;
const BALL_SPEED: f32 = 200.0;

#[derive(Component)]
#[require(Position, LinearVelocity)]
struct Paddle;

fn spawn_paddle(
    mut commands: Commands,
    mut meshes: ResMut<Assets<Mesh>>,
    mut materials: ResMut<Assets<ColorMaterial>>,
) {
    let shape = Rectangle::new(100.0, 20.0);
    let mesh = meshes.add(shape);
    let material = materials.add(Color::srgb_u8(0x00, 0xff, 0xff));

    commands.spawn((
        Paddle,
        Position(Vec2::new(0.0, -(SCREEN_HEIGHT as f32) / 2.0 + 40.0)),
        Mesh2d(mesh),
        MeshMaterial2d(material),
        RigidBody::Kinematic,
        Collider::from(shape),
    ));
}

fn handle_player_input(
    mut paddle_velocity: Single<&mut LinearVelocity, (With<Paddle>, Without<Ball>)>,
    keyboard_input: Res<ButtonInput<KeyCode>>,
) {
    let mut input = Vec2::ZERO;
    if keyboard_input.pressed(KeyCode::ArrowLeft) {
        input += Vec2::NEG_X;
    }

    if keyboard_input.pressed(KeyCode::ArrowRight) {
        input += Vec2::X;
    }

    input *= PADDLE_SPEED;
    input = input.adjust_precision();
    paddle_velocity.0 = input;
}

#[derive(Component)]
#[require(Position, LinearVelocity)]
struct Ball;

fn spawn_ball(
    mut commands: Commands,
    mut meshes: ResMut<Assets<Mesh>>,
    mut materials: ResMut<Assets<ColorMaterial>>,
) {
    let shape = Circle::new(10.0);
    let mesh = meshes.add(shape);
    let material = materials.add(Color::srgb_u8(0x00, 0xff, 0xff));

    commands.spawn((
        Ball,
        Mesh2d(mesh),
        MeshMaterial2d(material),
        Collider::from(shape),
        RigidBody::Dynamic,
        Restitution::PERFECTLY_ELASTIC.with_combine_rule(CoefficientCombine::Max),
        GravityScale(0.0),
        LockedAxes::ROTATION_LOCKED,
    ));
}

fn log_positions(
    paddle_position: Single<&Position, (With<Paddle>, Without<Ball>)>,
    ball_position: Single<&Position, With<Ball>>,
) {
    log::info!("Paddle position: {}", paddle_position.0);
    log::info!("Ball position: {}", ball_position.0);
}

fn spawn_walls(mut commands: Commands, window: Single<&Window>) {
    let width = window.resolution.width();
    let height = window.resolution.height();
    let thickness = 10.0;

    let width_2 = width / 2.0;
    let height_2 = height / 2.0;
    let thickness_2 = thickness / 2.0;

    let x = width_2 + thickness_2;
    let y = height_2 + thickness_2;
    let left_collider = Collider::rectangle(thickness, height);
    let top_collider = Collider::rectangle(width, thickness);

    commands.spawn((
        RigidBody::Static,
        Collider::compound(vec![
            // Top-wall
            (Vec2::new(0.0, y), 0.0, top_collider.clone()),
            // Bottom-wall
            (Vec2::new(0.0, -y), 0.0, top_collider),
            // Left-wall
            (Vec2::new(-x, 0.0), 0.0, left_collider.clone()),
            // Right-wall
            (Vec2::new(x, 0.0), 0.0, left_collider),
        ]),
    ));
}

fn spawn_camera(mut commands: Commands) {
    commands.spawn(Camera2d);
}

pub fn main() -> AppExit {
    let mut app = App::new();
    let app_window = Window {
        title: "Breakout".into(),
        resizable: false,
        resolution: (SCREEN_WIDTH, SCREEN_HEIGHT).into(),
        canvas: Some("#bevy".to_owned()),
        fit_canvas_to_parent: true,
        ..Default::default()
    };

    // Plugins
    app.add_plugins((
        DefaultPlugins.set(WindowPlugin {
            primary_window: Some(app_window),
            ..Default::default()
        }),
        PhysicsPlugins::default(),
    ));

    #[cfg(debug_assertions)]
    app.add_plugins(PhysicsDebugPlugin);

    // Startup systems
    app.add_systems(
        Startup,
        (spawn_camera, spawn_paddle, spawn_ball, spawn_walls),
    );

    // Fixed update systems
    app.add_systems(FixedUpdate, (handle_player_input, log_positions).chain());

    app.run()
}
