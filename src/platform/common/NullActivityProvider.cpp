#include "platform/Factory.h"
#include <memory>
namespace pcat { class Null final:public IActivityProvider{ActivitySnapshot capture()override{return {};}ProviderCapabilities capabilities()const override{return {false,false,false,false,"Unsupported platform"};}};std::unique_ptr<IActivityProvider> createActivityProvider(){return std::make_unique<Null>();}}
