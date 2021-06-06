# th6hook
This is my code from when I was trying to train stable-baselines to beat Touhou 6.

I kept coming back to it over a couple of months, but I never got it to actually learn anything apart from just camping in the corner of the screen.

However, I think the actual agent framework should be fine for anyone who actually knows anything about machine learning, so I think it's still worth pushing here.

The code is a bit messy and undocumented, but the gist of getting it to work is fixing the hardcoded paths in the .py files and injecting an import to the compiled th6hook.dll with Stud-PE.