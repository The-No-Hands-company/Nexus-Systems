# NexusLang Business Application Examples

This directory contains examples demonstrating NexusLang for business and enterprise applications.

## Examples

1. **inventory_system.nxl** - Product inventory management with stock tracking
2. **task_tracker_dashboard.nxl** - Team task workflow with KPI dashboard reporting
3. **service_desk_tui.nxl** - Structured terminal application with SLA-focused operations dashboard
4. **../27_delivery_dashboard_gui.nxl** - Desktop GUI KPI dashboard with progress bars and in-app PNG capture

## Running

- CLI/TUI showcase:
	- `PYTHONPATH=src python -m nexuslang.main examples/business/service_desk_tui.nxl`
- Business workflow showcase:
	- `PYTHONPATH=src python -m nexuslang.main examples/business/task_tracker_dashboard.nxl`
- GUI showcase:
	- `PYTHONPATH=src python -m nexuslang.main examples/27_delivery_dashboard_gui.nxl`

## Screenshots

### Delivery Command Center GUI

![Delivery Command Center GUI](../../showcase/delivery/delivery_command_center_gui.png)

### Service Desk Interactive TUI

![Service Desk Interactive TUI](../../showcase/delivery/service_desk_tui_console.png)

## Domain Coverage

These examples showcase NLPL's capability in:
- Business logic and calculations
- Data validation and processing
- Report generation
- CRUD operations for business entities
- Financial computations
- Team workflow and delivery analytics
- CLI/TUI application presentation for operations workflows
- Desktop GUI dashboards suitable for screenshot-based showcase assets

NLPL is equally capable of business applications, web services, data processing, scientific computing, and system programming.
