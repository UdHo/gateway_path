use eframe::egui;
use egui_plot::{Line, Plot, PlotBounds, PlotPoint, PlotPoints, Points, Polygon};
use serde::{Deserialize, Serialize};

const URL: &str = "http://localhost:8080/shortest-path";
const GRAB_RADIUS_PX: f32 = 14.0;

#[derive(Clone, Copy, Serialize, Deserialize)]
struct Pt {
    x: f64,
    y: f64,
}

impl Pt {
    const fn new(x: f64, y: f64) -> Self {
        Pt { x, y }
    }

    fn xy(self) -> [f64; 2] {
        [self.x, self.y]
    }
}

#[derive(Clone, Serialize)]
struct Segment {
    left: Pt,
    right: Pt,
}

#[derive(Serialize)]
struct Request<'a> {
    from: Pt,
    to: Pt,
    segments: &'a [Segment],
}

#[derive(Default, Deserialize)]
struct Response {
    path: Vec<Pt>,
}

#[derive(Clone, Copy)]
enum Handle {
    From,
    To,
    Left(usize),
    Right(usize),
}

/// Example from the orca challenge.
fn example_segments() -> Vec<Segment> {
    [
        [(5.0, 3.0), (9.68, 3.82)],
        [(2.8, 6.4), (7.36, 7.44)],
        [(4.06, 11.84), (8.24, 9.7)],
        [(6.82, 14.04), (11.36, 12.4)],
        [(4.88, 15.36), (14.24, 14.24)],
        [(12.58, 18.0), (17.02, 10.48)],
        [(20.24, 15.74), (18.38, 11.64)],
        [(21.58, 13.92), (22.22, 7.24)],
        [(23.84, 15.28), (25.7, 10.76)],
        [(30.1, 11.76), (25.82, 8.52)],
        [(29.282, 6.866), (25.938, 6.91)],
        [(31.042, 4.116), (28.0, 4.0)],
    ]
    .map(|[(lx, ly), (rx, ry)]| Segment {
        left: Pt::new(lx, ly),
        right: Pt::new(rx, ry),
    })
    .to_vec()
}

struct App {
    from: Pt,
    to: Pt,
    segments: Vec<Segment>,
    result: Response,
    error: Option<String>,
    dragged: Option<Handle>,
    first_frame: bool,
}

impl App {
    fn new() -> Self {
        let mut app = App {
            from: Pt::new(6.0, 2.0),
            to: Pt::new(30.0, 2.0),
            segments: example_segments(),
            result: Response::default(),
            error: None,
            dragged: None,
            first_frame: true,
        };
        app.solve();
        app
    }

    fn handles(&self) -> Vec<(Handle, Pt)> {
        let mut all = vec![(Handle::From, self.from), (Handle::To, self.to)];
        for (i, s) in self.segments.iter().enumerate() {
            all.push((Handle::Left(i), s.left));
            all.push((Handle::Right(i), s.right));
        }
        all
    }

    fn set(&mut self, handle: Handle, p: Pt) {
        match handle {
            Handle::From => self.from = p,
            Handle::To => self.to = p,
            Handle::Left(i) => self.segments[i].left = p,
            Handle::Right(i) => self.segments[i].right = p,
        }
    }

    fn add_segment(&mut self) {
        let x = self.segments.last().map_or(self.from.x, |s| s.left.x) + 2.0;
        self.segments.push(Segment {
            left: Pt::new(x, self.to.y + 3.0),
            right: Pt::new(x, self.to.y - 1.0),
        });
    }

    fn solve(&mut self) {
        let request = Request {
            from: self.from,
            to: self.to,
            segments: &self.segments,
        };
        let response = ureq::post(URL)
            .send_json(&request)
            .and_then(|mut r| r.body_mut().read_json::<Response>());
        match response {
            Ok(result) => (self.result, self.error) = (result, None),
            Err(e) => self.error = Some(e.to_string()),
        }
    }

    fn toolbar(&mut self, ui: &mut egui::Ui) -> bool {
        let mut changed = false;
        ui.horizontal(|ui| {
            if ui.button("Add segment").clicked() {
                self.add_segment();
                changed = true;
            }
            if ui.button("Remove last").clicked() {
                changed |= self.segments.pop().is_some();
            }
            if let Some(e) = &self.error {
                ui.colored_label(egui::Color32::RED, e);
            }
        });
        changed
    }

    /// Returns true if a handle was moved.
    fn drag(&mut self, plot_ui: &egui_plot::PlotUi) -> bool {
        let response = plot_ui.response();
        if response.drag_started() {
            let transform = plot_ui.transform();
            self.dragged = response.interact_pointer_pos().and_then(|pos| {
                self.handles()
                    .into_iter()
                    .map(|(h, p)| {
                        let screen = transform.position_from_point(&PlotPoint::new(p.x, p.y));
                        (h, screen.distance(pos))
                    })
                    .filter(|(_, d)| *d < GRAB_RADIUS_PX)
                    .min_by(|a, b| a.1.total_cmp(&b.1))
                    .map(|(h, _)| h)
            });
        }
        if response.drag_stopped() {
            self.dragged = None;
        }
        match (self.dragged, plot_ui.pointer_coordinate()) {
            (Some(handle), Some(p)) if response.dragged() => {
                self.set(handle, Pt::new(p.x, p.y));
                true
            }
            _ => false,
        }
    }

    /// Outline of the tunnel: from, all left points, to, all right points reversed.
    fn outline(&self) -> Vec<[f64; 2]> {
        let lefts = self.segments.iter().map(|s| s.left);
        let rights = self.segments.iter().rev().map(|s| s.right);
        [self.from]
            .into_iter()
            .chain(lefts)
            .chain([self.to])
            .chain(rights)
            .chain([self.from])
            .map(Pt::xy)
            .collect()
    }

    /// egui can only fill convex polygons, so the tunnel is filled as a strip
    /// of triangles between consecutive gateways.
    fn draw_tunnel(&self, plot_ui: &mut egui_plot::PlotUi) {
        let color = egui::Color32::from_rgb(80, 140, 220);
        let fill = color.gamma_multiply(0.15);
        let gateways: Vec<[Pt; 2]> = [[self.from; 2]]
            .into_iter()
            .chain(self.segments.iter().map(|s| [s.left, s.right]))
            .chain([[self.to; 2]])
            .collect();
        for pair in gateways.windows(2) {
            let ([al, ar], [bl, br]) = (pair[0], pair[1]);
            for triangle in [[al, ar, br], [al, br, bl]] {
                let points: Vec<_> = triangle.iter().map(|p| p.xy()).collect();
                plot_ui.polygon(
                    Polygon::new("", points)
                        .fill_color(fill)
                        .stroke(egui::Stroke::NONE),
                );
            }
        }
        plot_ui.line(Line::new("", self.outline()).color(color));
    }

    fn draw(&self, plot_ui: &mut egui_plot::PlotUi) {
        self.draw_tunnel(plot_ui);
        let dot = |p: Pt, radius: f32, color: egui::Color32| {
            Points::new("", vec![p.xy()]).radius(radius).color(color)
        };
        for s in &self.segments {
            let line = Line::new("", vec![s.left.xy(), s.right.xy()]);
            plot_ui.line(line.color(egui::Color32::GRAY));
            plot_ui.points(dot(s.left, 6.0, egui::Color32::LIGHT_RED));
            plot_ui.points(dot(s.right, 6.0, egui::Color32::LIGHT_BLUE));
        }
        let path: PlotPoints = self.result.path.iter().map(|p| p.xy()).collect();
        plot_ui.line(Line::new("", path).width(2.5).color(egui::Color32::GREEN));
        plot_ui.points(dot(self.from, 8.0, egui::Color32::YELLOW));
        plot_ui.points(dot(self.to, 8.0, egui::Color32::WHITE));
    }
}

impl eframe::App for App {
    fn ui(&mut self, ui: &mut egui::Ui, _frame: &mut eframe::Frame) {
        let mut changed = self.toolbar(ui);
        let first_frame = std::mem::take(&mut self.first_frame);
        Plot::new("gateways")
            .data_aspect(1.0)
            .allow_drag(false)
            .allow_zoom(false)
            .allow_scroll(false)
            .allow_boxed_zoom(false)
            .show(ui, |plot_ui| {
                if first_frame {
                    plot_ui.set_plot_bounds(PlotBounds::from_min_max([0.0, 0.0], [36.0, 20.0]));
                }
                changed |= self.drag(plot_ui);
                self.draw(plot_ui);
            });
        if changed {
            self.solve();
        }
    }
}

fn main() -> eframe::Result {
    eframe::run_native(
        "Gateway editor",
        eframe::NativeOptions::default(),
        Box::new(|_| Ok(Box::new(App::new()))),
    )
}
