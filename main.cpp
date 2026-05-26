// Copyright (C) 2026 mxreal64
// Licensed under the GPL-3.0 License

import std;
import TerminalWorkspace;

int main(int argc, char* argv[]) {
    std::string initial_file = "";
    if (argc > 1) {
        initial_file = argv[1];
    }
    
    MuxUI::Workspace app(initial_file);
    app.run();
    return 0;
}
