# Shared To-Do List — C++ Server (TCP) + C# WPF Client

## Summary
This repo contains a simple multi-client shared to-do list:
- **Server**: C++ (Winsock), maintains in-memory list, broadcasts updates.
- **Client**: C# WPF (.NET 8.0 / 6.0 / 7.0), displays list, adds items, toggles status, real-time updates.

Messages are newline-delimited JSON (see `protocol.md`).

---

## Prerequisites (Windows)
- Visual Studio 2022 or later.
  - Workloads: **Desktop development with C++** and **.NET desktop development**.
- .NET SDK (if Visual Studio doesn't include it). Using `net8.0-windows` in project; `net6.0-windows` or `net7.0-windows` also supported.
- (C++) `json.hpp` from nlohmann (download single-header and add to project).
- No external C++ package manager is required for this simple example.


---

## Build & Run — Server (C++)
1. Open Visual Studio and open the solution `TodoAppSolution.sln`
2. Ensure `json.hpp` (nlohmann) is placed in `TodoServer` and added to project.
3. In project properties:
   - C/C++ → Language → C++ Language Standard → **/std:c++17**.
   - C/C++ → Precompiled Headers → **Not Using Precompiled Headers** (if you get pch errors).
4. Build `TodoServer` (Build → Build Solution).
5. Run `TodoServer` project (Set as Startup Project → Start Without Debugging (Ctrl+F5)). 
   Console will show: Server listening on port 5000 ...
   The server listens on port **5000**.

---

## Build & Run — Client (C# WPF)
1. Open the `TodoClient` project in the solution (or add it as a new WPF project).
2. Ensure the `TargetFramework` in `TodoClient.csproj` matches a framework you have (`net8.0-windows` or `net6.0-windows`).
3. Install NuGet package **Newtonsoft.Json**:
- Right-click `TodoClient` → Manage NuGet Packages → Browse → `Newtonsoft.Json` → Install.
4. Build `TodoClient` and run the WPF app (F5).
5. On startup the client auto-connects to `127.0.0.1:5000` and requests the current list.

### Running multiple clients
- Build the client, then run one instance from Visual Studio and start other instances directly from the output folder:
`TodoClient\bin\Debug\net8.0-windows\TodoClient.exe`
- Actions (Add / Toggle) on one client will reflect on others in real-time.

---

## Quick test steps
1. Start server (`TodoServer`).
2. Start Client A.
3. Type `Buy milk` in the textbox, click **Add** → item appears in Client A.
4. Start Client B (new instance) → Client B receives the list and shows `Buy milk`.
5. Toggle the checkbox in Client B → both clients update to show Completed.

---