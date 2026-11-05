// SPDX-License-Identifier: MIT
// Algorithm design after AutoLevel by Al-Asl (MIT).
// Included after al_app.cpp in the SDK unity build.
namespace alui {
void startJob(bool selection,bool group,bool validate){
 if(busy())return;if(!currentBuilder()&&!validate)throw std::runtime_error("create a builder first");
 auto rr=repos;auto bb=builders;auto saved=settings;auto cache=compiled;int bi=builderIndex,ri=repoIndex,gi=groupIndex,profile=int(host->number("rb.profile"));char tex[512]{};host->string("ca_newbrushtexture",tex,sizeof tex);std::string material=rbgen::material(tex);cancel=false;progress=0;pendingKey=stateKey();message=validate?"Compiling repo...":"Building in the background...";
 job=std::async(std::launch::async,[rr,bb,saved,cache,bi,ri,gi,profile,material,selection,group,validate]()mutable{
  Job out;out.validate=validate;try{
   auto runtime=[&](const std::string& uid){auto r=std::find_if(rr.begin(),rr.end(),[&](auto& r){return r.uid==uid;});if(r==rr.end())throw std::runtime_error("assign BlockRepo to start editing");auto hash=textHash(serialize(*r));auto old=cache.find(uid);if(old==cache.end()||old->second->repoHash!=hash)cache[uid]=std::make_shared<RepoRuntime>(compileRepo(*r,&cancel));return cache.at(uid);};
   if(validate){auto r=runtime(rr.at(ri).uid);for(auto& b:bb)if(b.repoUid==r->source.uid)integrity(b,*r);int missing=0;for(auto& d:r->conn)for(auto& links:d)missing+=links.empty();out.message="Repo valid: "+std::to_string(r->states.size())+" states, "+std::to_string(r->layerRange.size())+" layers, "+std::to_string(missing)+" unconnected sides, "+std::to_string(r->warnings.size())+" duplicate warnings.";}
   else{
    std::vector<size_t> targets;if(group){auto groups=groupBuilders(bb);if(gi<0||size_t(gi)>=groups.size())throw std::runtime_error("select a builder group");for(auto i:groups[gi])if(bb[i].selected)targets.push_back(i);}else targets={size_t(bi)};if(targets.empty())throw std::runtime_error("no builders selected");
    std::map<std::string,std::shared_ptr<const RepoRuntime>> runtimeMap;for(auto i:targets){auto r=runtime(bb[i].repoUid);runtimeMap[bb[i].repoUid]=r;int current=bb[i].currentLayer;integrity(bb[i],*r);bb[i].currentLayer=std::clamp(current,0,int(r->layerRange.size())-1);}
    std::vector<SolveResult> results;auto began=std::chrono::steady_clock::now();
    if(group){auto gr=solveBuilders(bb,runtimeMap,targets,saved,&cancel,&progress);if(!gr.success)throw std::runtime_error(gr.message);results=gr.results;}
    else{auto& b=bb[bi];auto r=runtimeMap.at(b.repoUid);auto q=builderRequest(*r,b,bb,saved,selection,&cancel,&progress);int last=b.rebuildBottomLayers&&b.autoAdvanceLayers?int(r->layerRange.size())-1:b.currentLayer,first=b.rebuildBottomLayers?0:b.currentLayer;for(int l=first;l<=last;l++){q.layer=l;auto result=solve(q);if(!result.iterationsUsed)throw std::runtime_error("build failed at layer "+std::to_string(l)+"; first contradiction at cell ("+std::to_string(result.failCell.x)+","+std::to_string(result.failCell.y)+","+std::to_string(result.failCell.z)+") during "+result.failStage);b.lastSeed=int(result.seedUsed);results.push_back(result);}b.currentLayer=last;}
    out.build.map="{\n\"classname\" \"worldspawn\"\n}\n";int ids=1000;
    for(auto i:targets){auto map=buildMap(*runtimeMap.at(bb[i].repoUid),bb[i],saved,profile,material,&cancel,ids);ids+=40000;out.build.map+=map.map.substr(map.map.find("}\n")+2);out.build.lines.insert(out.build.lines.end(),map.lines.begin(),map.lines.end());out.build.brushes+=map.brushes;out.build.modules+=map.modules;out.build.warnings.insert(out.build.warnings.end(),map.warnings.begin(),map.warnings.end());}
    if(out.build.map.size()>16*1024*1024||out.build.brushes>16000)throw std::runtime_error("group exceeds import budget");if(out.build.lines.size()>16384*6)out.build.lines.resize(16384*6);
    auto& first=results.front();out.message="build succeeded ,completion time "+std::to_string(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-began).count())+" ms, number of iterations "+std::to_string(first.iterationsUsed)+", number of blocks "+std::to_string(runtimeMap.begin()->second->states.size())+"; seed "+std::to_string(first.seedUsed)+", coverage "+std::to_string(first.quality.coverage*100)+"%, floors "+std::to_string(first.quality.floors)+"/"+std::to_string(bb[bi].levelData.size.z)+", "+std::to_string(out.build.brushes)+" brushes";
    for(auto result:results)if(!result.message.empty())out.message+="\n"+result.message;for(auto warning:out.build.warnings)out.message+="\n"+warning;
   }
   out.builders=std::move(bb);out.compiled=std::move(cache);
  }catch(const std::exception& e){out.error=e.what();}return out;
 });
}
static void tick(){if(!ready)return;initialize();try{
 if(job.valid()&&job.wait_for(std::chrono::seconds(0))==std::future_status::ready){auto out=job.get();if(pendingKey!=stateKey()){message="Document/settings changed during generation. Generate a fresh preview.";clearPreview();}else if(!out.error.empty())message="ERROR: "+out.error;else{compiled=std::move(out.compiled);builders=std::move(out.builders);revision++;hashRevision=0;if(!out.validate){preview=std::move(out.build);previewKey=stateKey();host->preview_lines(preview.lines.data(),preview.lines.size()/6);}message=out.message;}}
 receiveCapture();if(!preview.map.empty()&&previewKey!=stateKey()){clearPreview();message="Document/settings changed; preview invalidated.";}if(projectHash()!=savedHash&&!busy()&&std::chrono::steady_clock::now()-editedAt>std::chrono::seconds(2))saveAll();viewport();
 }catch(const std::exception& e){error(e);}}
static void header(){const char* names[]={"Level Builder","Blocks Repo","Block Asset","Big Block","Builders Window"};for(int i=0;i<5;i++){if(i&&i%3)host->same_line();Disable disabled(i==page);if(button((i==page?"> ":"")+std::string(names[i])+"###autolevel-page-"+std::to_string(i))){page=i;changed();}}host->separator(names[page]);std::vector<std::string> bs;for(auto& b:builders)bs.push_back(b.name);combo("Builder",builderIndex,bs);
 if(button("New##builder")){if(auto r=currentRepo()){auto b=defaultBuilder(*r);b.uid=uniqueId("builder");b.name="Dungeon "+std::to_string(builders.size()+1);builders.push_back(b);builderIndex=builders.size()-1;changed();}}host->same_line();
 if(button("New empty")){if(auto r=currentRepo()){auto b=defaultBuilder(*r,true);b.uid=uniqueId("builder");builders.push_back(b);builderIndex=builders.size()-1;changed();}}host->same_line();
 if(button("Duplicate##builder")&&currentBuilder()){auto copy=*currentBuilder();copy.uid=uniqueId("builder");copy.name+=" copy";for(auto& boundary:copy.boundary)boundary.levelBuilderUid.clear();builders.push_back(copy);builderIndex=builders.size()-1;changed();}host->same_line();
 if(button("Delete##builder")&&currentBuilder()){auto uid=currentBuilder()->uid;builders.erase(builders.begin()+builderIndex);for(auto& b:builders)for(auto& side:b.boundary)if(side.levelBuilderUid==uid)side.levelBuilderUid.clear();builderIndex=0;changed();}if(auto b=currentBuilder())textInput("Name##builder",b->name);
}
static void draw(){if(!ready){if(AL_HAS(text))host->text("This editor build is too old for AutoLevel");return;}initialize();try{
 {Disable disabled(busy());header();switch(page){case 0:drawBuilder();break;case 1:drawRepo();break;case 2:drawBlock();break;case 3:drawBig();break;case 4:drawBuilders();break;}}
 if(busy()){host->progress(progress.load(),"AutoLevel");if(button("Cancel")){cancel=true;captureMode=0;}}if(!message.empty())line(message);
 }catch(const std::exception& e){error(e);line(message);}}
static void shutdown(){cancel=true;if(job.valid()){job.wait();job.get();}captureMode=0;try{saveAll();}catch(const std::exception& e){if(AL_HAS(log))host->log(e.what());}clearPreview();initialized=false;}
}
