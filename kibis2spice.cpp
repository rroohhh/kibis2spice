class KIBIS_MODEL;
class KIBIS_PARAMETER;

#include <filesystem>
#include <string>

std::string dieModelName(KIBIS_MODEL model, KIBIS_PARAMETER param);

#include "kibis/ibis_parser.h"
#include "kibis/kibis.h"

#include "kibis/ibis_parser.cpp"
#include "kibis/kibis.cpp"

#include "kicad_compat/reporter.cpp"

std::string dieModelName(KIBIS_MODEL model, KIBIS_PARAMETER param) {
    return model.m_name + "_" + std::to_string(param.m_Rpin) + "_" + std::to_string(param.m_Lpin) + "_" +
           std::to_string(param.m_Cpin) + "_" + std::to_string(param.m_Ccomp) + "_" + std::to_string(param.m_supply) +
           "_" + std::to_string((size_t)param.m_waveform) + "_" + std::to_string((int)param.m_accuracy);
}

std::string doubleToString(double aNumber) {
    std::ostringstream ss;
    ss.setf(std::ios_base::scientific, std::ios_base::floatfield);
    ss << aNumber;
    return ss.str();
}

#include <map>

// The plan:
// build a complete model using
// .param for all the parameters
// and .if to select the correct subckt
// buuuuut we have to map params to numbers, because spice sucks, how unfortunate
// we probably want to build the whole symbol
// and just have selectors for the submodels?
// package is probably not doable, because that can change the actual pins
// also min / max / typ corner!! (need to figure out how to map the different component corners to a combined corner)

struct KuKdTInfo {
    std::vector<double> ku, kd, t;
};

class CACHED_KIBIS_PIN : public KIBIS_PIN {
private:
    static std::map<std::string, KuKdTInfo> CACHE;

public:
    CACHED_KIBIS_PIN(const KIBIS_PIN & pin) : KIBIS_PIN::KIBIS_PIN(pin) {}

    void getKuKdFromFile(std::string * aSimul) override {
        auto kv = CACHED_KIBIS_PIN::CACHE.find(*aSimul);
        if(kv != CACHED_KIBIS_PIN::CACHE.end()) {
            auto data = kv->second;
            m_Kd      = data.kd;
            m_Ku      = data.ku;
            m_t       = data.t;
        } else {
            KIBIS_PIN::getKuKdFromFile(aSimul);
            CACHED_KIBIS_PIN::CACHE[*aSimul] = KuKdTInfo{m_Ku, m_Kd, m_t};
        }
    }
};

std::map<std::string, KuKdTInfo> CACHED_KIBIS_PIN::CACHE;

std::string pinModelName(std::string pinName, IBIS_CORNER corner, std::string modelName) {
    return std::string("_INTERNAL_") + pinName + "_CORNER_" + std::to_string(corner) + "_MODEL_" + modelName;
}

void print_usage() {
    printf("Usage:\n"
           "  kibis2spice [options] IBIS_MODEL OUTPUT_DIR\n"
           "OPTIONS\n"
           "  -b bits       number of bits of prbs\n"
           "  -d delay      delay before output happens in seconds\n"
           "  -f frequency  frequency of prbs\n");
}

int main(int argc, char ** argv) {
    auto frequency = 10e6;
    auto bits      = 10;
    auto delay     = 0.0;

    for(;;) {
        switch(getopt(argc, argv, "b:d:f:h")) {
        case 'b':
            bits = std::stoi(optarg);
            continue;

        case 'd':
            delay = std::stod(optarg);
            continue;

        case 'f':
            frequency = std::stod(optarg);
            continue;

        case '?':
        case 'h':
        default:
            print_usage();
            break;

        case -1:
            break;
        }

        break;
    }

    argc -= optind;
    argv += optind;

    if(argc != 2) {
        print_usage();
        std::exit(EXIT_FAILURE);
    }

    REPORTER reporter;
    auto     file       = std::string(argv[0]);
    auto     output_dir = std::string(argv[1]);
    std::filesystem::create_directory(output_dir);
    KIBIS kibis(file, &reporter);

    IbisParser parser(&reporter);
    parser.m_parrot = false;
    parser.ParseFile(file);

    KIBIS_WAVEFORM_PRBS * waveform = new KIBIS_WAVEFORM_PRBS(&kibis);
    waveform->m_bitrate            = frequency;
    waveform->m_delay              = delay;
    waveform->m_bits               = bits;

    KIBIS_PARAMETER kparams;
    kparams.m_waveform = waveform;

    std::map<std::string, std::vector<IbisModelSelectorEntry>> selector_to_submodels;

    for(auto & selector : parser.m_ibisFile.m_modelSelectors) {
        selector_to_submodels.insert({selector.m_name, selector.m_models});
    }

    // gives pin name and model name
    std::map<std::string, std::vector<std::tuple<std::string, std::string, std::string>>>
        component_to_pins_without_selection;
    // give a list of selector name, selection options and pin names
    std::map<std::string, std::vector<std::tuple<std::string, std::vector<IbisModelSelectorEntry>,
                                                 std::vector<std::pair<std::string, std::string>>>>>
        component_to_selectors_and_pins;

    for(auto & comp : parser.m_ibisFile.m_components) {
        auto                                                           comp_name = comp.m_name;
        std::vector<std::tuple<std::string, std::string, std::string>> pins_without_selection;
        // selector -> pin name
        std::map<std::string, std::vector<std::pair<std::string, std::string>>> selector_to_pin;

        for(auto & pin : comp.m_pins) {
            if(pin.m_dummy) { continue; }
            // only consider pins that are actually modeled
            if((pin.m_modelName != "POWER") and (pin.m_modelName != "GND") and (pin.m_modelName != "NC")) {
                auto models = selector_to_submodels.find(pin.m_modelName);

                // this means there is either no selector for this model, or only one choice, so we can always
                // instantiate this
                if((models == selector_to_submodels.end()) or (models->second.size() == 1)) {
                    auto modelName = pin.m_modelName;
                    if(models != selector_to_submodels.end()) { modelName = models->second[0].m_modelName; }
                    pins_without_selection.push_back({pin.m_pinName, pin.m_signalName, modelName});
                } else { // there is a selection to be taken, we want to group those after the selection
                    selector_to_pin[pin.m_modelName].push_back({pin.m_pinName, pin.m_signalName});
                }
            }
        }

        component_to_pins_without_selection[comp_name] = pins_without_selection;
        std::vector<std::tuple<std::string, std::vector<IbisModelSelectorEntry>,
                               std::vector<std::pair<std::string, std::string>>>>
            selectors_and_pins;

        for(auto & [selector, pins] : selector_to_pin) {
            selectors_and_pins.push_back({selector, selector_to_submodels[selector], pins});
        }
        component_to_selectors_and_pins[comp_name] = selectors_and_pins;
    }

    // we generate one model for each component
    // different components can have different pins, so they always need different models / subcircuits
    for(auto & comp : kibis.m_components) {
        auto comp_name              = comp.m_name;
        auto pins_without_selection = component_to_pins_without_selection[comp_name];
        auto selections_and_pins    = component_to_selectors_and_pins[comp_name];

        // generate a help string to help the user set the model selectors and the corner
        std::string component_model;
        component_model += "* model parameters:\n";
        component_model += "*   frequency: " + std::to_string(frequency) + "\n";
        component_model += "*   delay: " + std::to_string(delay) + "\n";
        component_model += "*   bits: " + std::to_string(bits) + "\n";
        component_model += "* parameters:";
        component_model += "* " + comp_name + " model, select following parameters\n";
        component_model += "* corner: " + std::to_string(IBIS_CORNER::TYP) + " = TYP, " +
                           std::to_string(IBIS_CORNER::MIN) + " = MIN, " + std::to_string(IBIS_CORNER::MAX) +
                           " = MAX\n";

        std::vector<std::pair<std::string, unsigned>> selectors_to_consider;

        for(auto & [selector, models, _] : selections_and_pins) {
            component_model += "* " + selector + ":\n";
            int i = 0;
            for(auto & model : models) {
                component_model +=
                    "*   " + std::to_string(i++) + " = " + model.m_modelName + "(" + model.m_modelDescription + ")\n";
            }
        }

        component_model += ".subckt " + comp_name + " GND";

        for(auto & [_1, name, _2] : pins_without_selection) { component_model += " " + name; }

        for(auto & [_1, _2, pins] : selections_and_pins) {
            for(auto & [_3, name] : pins) { component_model += " " + name; }
        }

        component_model += " corner=0";
        for(auto & [selector, _1, _2] : selections_and_pins) { component_model += " " + selector + "=0"; }
        component_model += "\n";

        for(auto & corner : {IBIS_CORNER::TYP, IBIS_CORNER::MIN, IBIS_CORNER::MAX}) {
            component_model += ".if(corner==" + std::to_string(corner) + ")\n";

            // first the pins without a selection to be taken
            for(auto & [pinName, signalName, modelName] : pins_without_selection) {
                component_model +=
                    "x" + signalName + " " + signalName + " " + pinModelName(pinName, corner, modelName) + "\n";
            }

            for(auto & [selector, selections, pins] : selections_and_pins) {
                int i = 0;
                for(auto & selection : selections) {
                    component_model += "* " + selection.m_modelName + "(" + selection.m_modelDescription + ")\n";
                    component_model += ".if(" + selector + "==" + std::to_string(i++) + ")\n";
                    for(auto & [pinName, signalName] : pins) {
                        component_model += "x" + signalName + " GND " + signalName + " " +
                                           pinModelName(pinName, corner, selection.m_modelName) + "\n";
                    }
                    component_model += ".endif\n";
                }
            }

            component_model += ".endif\n";
        }

        std::map<std::string, std::string> dieModelNameToModel;

        for(auto & corner : {IBIS_CORNER::TYP, IBIS_CORNER::MIN, IBIS_CORNER::MAX}) {
            kparams.m_Rpin     = corner;
            kparams.m_Lpin     = corner;
            kparams.m_Cpin     = corner;
            kparams.m_Ccomp    = corner;
            kparams.m_supply   = corner;
            kparams.m_accuracy = KIBIS_ACCURACY::LEVEL_3;

            for(auto & non_cached_pin : comp.m_pins) {
                auto pin = CACHED_KIBIS_PIN(non_cached_pin);

                for(auto & model : pin.m_models) {
                    std::cerr << "doing component " << comp_name << " pin " << pin.m_signalName << " model "
                              << model->m_name << " corner " << corner << std::endl;

                    std::string dest;
                    if(model->m_type != IBIS_MODEL_TYPE::INPUT_STD) {
                        pin.writeSpiceDriver(&dest, pinModelName(pin.m_pinNumber, corner, model->m_name), *model,
                                             kparams);

                        auto thisDieModelName = dieModelName(*model, kparams);
                        auto dieModelEntry    = dieModelNameToModel.find(thisDieModelName);
                        if(dieModelEntry == dieModelNameToModel.end()) {
                            std::string dieModel;

                            dieModel += ".SUBCKT " + thisDieModelName + " DIE0 GND\n";
                            dieModel += "Vku KU GND pwl ( ";

                            auto m_Kd = pin.m_Kd;
                            auto m_Ku = pin.m_Ku;
                            auto m_t  = pin.m_t;

                            for(size_t i = 0; i < m_t.size(); i++) {
                                dieModel += doubleToString(m_t.at(i));
                                dieModel += " ";
                                dieModel += doubleToString(m_Ku.at(i));
                                dieModel += " ";
                            }

                            dieModel += ") \n";

                            dieModel += "Vkd KD GND pwl ( ";

                            for(size_t i = 0; i < m_t.size(); i++) {
                                dieModel += doubleToString(m_t.at(i));
                                dieModel += " ";
                                dieModel += doubleToString(m_Kd.at(i));
                                dieModel += " ";
                            }

                            dieModel += ") \n";

                            dieModel += model->SpiceDie(kparams, 0);

                            dieModel += ".ENDS\n";

                            dieModelNameToModel[thisDieModelName] = dieModel;
                        }
                    } else {
                        pin.writeSpiceDevice(&dest, pinModelName(pin.m_pinNumber, corner, model->m_name), *model,
                                             kparams);
                    }
                    component_model += dest;
                }
            }
        }

        for(auto & [_, model] : dieModelNameToModel) { component_model += model; }

        component_model += ".ends";

        std::ofstream output_file;
        output_file.open(output_dir + "/" + comp_name + ".spice");
        output_file << component_model;
    }
}
