# Description this file contains helper definitions for printing within make 
PRINT := printf

LOGO := logo.ansi.txt
MICROROBOTICS_LOGO := microrobotics.ansi.txt

### Functions ###
# Colorful text printing
NO_COL  := \033[0m
RED     := \033[0;31m
GREEN   := \033[0;32m
BLUE    := \033[0;34m
YELLOW  := \033[0;33m
BLINK   := \033[33;5m

# Print message with zero arguments (i.e. message)
define print0
  @$(PRINT) "$(GREEN)$(1)$(NO_COL)\n"
endef

# Print message with one argument (i.e. message arg)
define print1
  @$(PRINT) "$(GREEN)$(1) $(BLUE)$(2)$(NO_COL)\n"
endef

# Print message with two arguments (i.e. message arg1 -> arg2)
define print2
  @$(PRINT) "$(GREEN)$(1) $(YELLOW)$(2)$(GREEN) -> $(BLUE)$(3)$(NO_COL)\n"
endef


# Print message with two arguments (i.e. message arg1 -> arg2)
define todo1
  @$(PRINT) "$(RED)TODO: $(1) $(NO_COL)\n"
endef

define logo_with_text
  @$(PRINT) "\n$$(sed "9s|\(.*\)|\1    \\\033[0;33m$(1)\\\033[0m|" $(LOGO))\n"
endef

define microrobotics_logo_with_text
  $(PRINT) "\n$$(sed "6s|\(.*\)|\1    \\\033[0;33m$(1)\\\033[0m|" $(MICROROBOTICS_LOGO))\n"
endef