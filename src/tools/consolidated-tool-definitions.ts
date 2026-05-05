// AUTO-GENERATED FROM generated/tool-manifest.json — DO NOT EDIT.
// Regenerate via: python Scripts/run-cmd.py DumpMcpManifest && npm run gen:tool-defs
//

export interface ToolDefinition {
  category?: 'core' | 'world' | 'authoring' | 'gameplay' | 'utility';
  name: string;
  description: string;
  inputSchema: Record<string, unknown>;
  [key: string]: unknown;
}
export const consolidatedToolDefinitions: ToolDefinition[] = [
  {
    "name": "animation_physics",
    "description": "Create animation blueprints, blend spaces, montages, state machines, Control Rig, IK rigs, ragdolls, and vehicle physics.",
    "category": "gameplay",
    "inputSchema": {
      "type": "object",
      "properties": {
        "action": {
          "type": "string",
          "description": "Action",
          "enum": [
            "create_animation_blueprint",
            "create_animation_bp",
            "create_anim_blueprint",
            "create_blend_space",
            "create_blend_space_1d",
            "create_blend_space_2d",
            "create_blend_tree",
            "create_procedural_anim",
            "create_aim_offset",
            "add_aim_offset_sample",
            "create_state_machine",
            "add_state_machine",
            "add_state",
            "add_transition",
            "set_transition_rules",
            "add_blend_node",
            "add_cached_pose",
            "add_slot_node",
            "create_control_rig",
            "add_control",
            "add_rig_unit",
            "connect_rig_elements",
            "create_ik_rig",
            "add_ik_chain",
            "setup_ik",
            "create_pose_library",
            "create_animation_asset",
            "create_animation_sequence",
            "set_sequence_length",
            "add_bone_track",
            "set_bone_key",
            "set_curve_key",
            "create_montage",
            "add_montage_section",
            "add_montage_slot",
            "set_section_timing",
            "add_montage_notify",
            "set_blend_in",
            "set_blend_out",
            "link_sections",
            "add_notify",
            "play_montage",
            "play_anim_montage",
            "setup_ragdoll",
            "activate_ragdoll",
            "configure_vehicle",
            "setup_physics_simulation",
            "add_blend_sample",
            "set_axis_settings",
            "set_interpolation_settings",
            "setup_retargeting",
            "cleanup"
          ]
        },
        "name": {
          "type": "string",
          "description": "Name identifier."
        },
        "savePath": {
          "type": "string",
          "description": "Path to save the asset."
        },
        "skeletonPath": {
          "type": "string",
          "description": "Asset path (e.g., /Game/Path/Asset)."
        },
        "meshPath": {
          "type": "string",
          "description": "Mesh asset path."
        },
        "parentClass": {
          "type": "string"
        },
        "actorName": {
          "type": "string",
          "description": "Name of the actor."
        },
        "skeletonName": {
          "type": "string"
        },
        "targetSkeleton": {
          "type": "string",
          "description": "Asset path (e.g., /Game/Path/Asset)."
        },
        "blueprintName": {
          "type": "string"
        },
        "montageName": {
          "type": "string"
        },
        "animSequence": {
          "type": "string",
          "description": "Asset path (e.g., /Game/Path/Asset)."
        },
        "animPath": {
          "type": "string",
          "description": "Asset path (e.g., /Game/Path/Asset)."
        },
        "animAssetPath": {
          "type": "string",
          "description": "Asset path (e.g., /Game/Path/Asset)."
        },
        "animMontagePath": {
          "type": "string",
          "description": "Asset path (e.g., /Game/Path/Asset)."
        },
        "slotName": {
          "type": "string"
        },
        "sectionName": {
          "type": "string"
        },
        "notifyName": {
          "type": "string"
        },
        "boneName": {
          "type": "string",
          "description": "Name of the bone."
        },
        "curveName": {
          "type": "string"
        },
        "stateName": {
          "type": "string"
        },
        "machineName": {
          "type": "string"
        },
        "transitionName": {
          "type": "string"
        },
        "blendSpacePath": {
          "type": "string",
          "description": "Asset path (e.g., /Game/Path/Asset)."
        },
        "numSamples": {
          "type": "number"
        },
        "interpolationType": {
          "type": "string"
        },
        "axisName": {
          "type": "string"
        },
        "min": {
          "type": "number"
        },
        "max": {
          "type": "number"
        },
        "samples": {
          "type": "array",
          "items": {
            "type": "object"
          }
        },
        "animSequencePath": {
          "type": "string",
          "description": "Asset path (e.g., /Game/Path/Asset)."
        },
        "playRate": {
          "type": "number"
        },
        "frame": {
          "type": "number"
        },
        "time": {
          "type": "number"
        },
        "length": {
          "type": "number"
        },
        "location": {
          "type": "object",
          "description": "3D location (x, y, z).",
          "properties": {
            "x": {
              "type": "number"
            },
            "y": {
              "type": "number"
            },
            "z": {
              "type": "number"
            }
          }
        },
        "rotation": {
          "type": "object",
          "description": "3D rotation (pitch, yaw, roll).",
          "properties": {
            "pitch": {
              "type": "number"
            },
            "yaw": {
              "type": "number"
            },
            "roll": {
              "type": "number"
            }
          }
        },
        "scale": {
          "type": "object",
          "description": "3D scale (x, y, z).",
          "properties": {
            "x": {
              "type": "number"
            },
            "y": {
              "type": "number"
            },
            "z": {
              "type": "number"
            }
          }
        },
        "value": {
          "type": "object",
          "description": "Generic value (any type).",
          "additionalProperties": true
        },
        "enabled": {
          "type": "boolean",
          "description": "Whether the item/feature is enabled."
        },
        "rigPath": {
          "type": "string",
          "description": "Asset path (e.g., /Game/Path/Asset)."
        },
        "chainName": {
          "type": "string"
        },
        "startBone": {
          "type": "string",
          "description": "Name of the bone."
        },
        "endBone": {
          "type": "string",
          "description": "Name of the bone."
        },
        "controlName": {
          "type": "string"
        },
        "unitType": {
          "type": "string"
        },
        "sourceNode": {
          "type": "string"
        },
        "targetNode": {
          "type": "string"
        },
        "sourcePin": {
          "type": "string"
        },
        "targetPin": {
          "type": "string"
        },
        "vehicleType": {
          "type": "string"
        },
        "wheelConfig": {
          "type": "object",
          "additionalProperties": true
        },
        "engineTorque": {
          "type": "number"
        },
        "mass": {
          "type": "number"
        },
        "dragCoefficient": {
          "type": "number"
        },
        "artifacts": {
          "type": "array",
          "items": {
            "type": "string"
          }
        },
        "ragdollActive": {
          "type": "boolean"
        },
        "sourceSkeleton": {
          "type": "string",
          "description": "Asset path (e.g., /Game/Path/Asset)."
        },
        "retargetSkeleton": {
          "type": "string",
          "description": "Asset path (e.g., /Game/Path/Asset)."
        },
        "retargetProfile": {
          "type": "string"
        }
      },
      "required": [
        "action"
      ]
    }
  },
  {
    "name": "build_environment",
    "description": "Create/sculpt landscapes, paint foliage, and generate procedural terrain/biomes.",
    "category": "world",
    "inputSchema": {
      "type": "object",
      "properties": {
        "action": {
          "type": "string",
          "description": "Action",
          "enum": [
            "create_landscape",
            "sculpt",
            "sculpt_landscape",
            "add_foliage",
            "paint_foliage",
            "create_procedural_terrain",
            "create_procedural_foliage",
            "add_foliage_instances",
            "get_foliage_instances",
            "remove_foliage",
            "paint_landscape",
            "paint_landscape_layer",
            "modify_heightmap",
            "set_landscape_material",
            "create_landscape_grass_type",
            "generate_lods",
            "bake_lightmap",
            "export_snapshot",
            "import_snapshot",
            "delete",
            "create_sky_sphere",
            "set_time_of_day",
            "create_fog_volume"
          ]
        },
        "name": {
          "type": "string",
          "description": "Name identifier."
        },
        "landscapeName": {
          "type": "string"
        },
        "heightData": {
          "type": "array",
          "items": {
            "type": "number"
          }
        },
        "minX": {
          "type": "number"
        },
        "minY": {
          "type": "number"
        },
        "maxX": {
          "type": "number"
        },
        "maxY": {
          "type": "number"
        },
        "updateNormals": {
          "type": "boolean"
        },
        "location": {
          "type": "object",
          "description": "3D location (x, y, z).",
          "properties": {
            "x": {
              "type": "number"
            },
            "y": {
              "type": "number"
            },
            "z": {
              "type": "number"
            }
          }
        },
        "rotation": {
          "type": "object",
          "description": "3D rotation (pitch, yaw, roll).",
          "properties": {
            "pitch": {
              "type": "number"
            },
            "yaw": {
              "type": "number"
            },
            "roll": {
              "type": "number"
            }
          }
        },
        "scale": {
          "type": "object",
          "description": "3D scale (x, y, z).",
          "properties": {
            "x": {
              "type": "number"
            },
            "y": {
              "type": "number"
            },
            "z": {
              "type": "number"
            }
          }
        },
        "sizeX": {
          "type": "number"
        },
        "sizeY": {
          "type": "number"
        },
        "sectionSize": {
          "type": "number"
        },
        "sectionsPerComponent": {
          "type": "number"
        },
        "componentCount": {
          "type": "object",
          "description": "2D vector.",
          "properties": {
            "x": {
              "type": "number"
            },
            "y": {
              "type": "number"
            }
          }
        },
        "materialPath": {
          "type": "string",
          "description": "Material asset path."
        },
        "tool": {
          "type": "string"
        },
        "radius": {
          "type": "number"
        },
        "strength": {
          "type": "number"
        },
        "falloff": {
          "type": "number"
        },
        "brushSize": {
          "type": "number"
        },
        "layerName": {
          "type": "string"
        },
        "eraseMode": {
          "type": "boolean"
        },
        "actorName": {
          "type": "string",
          "description": "Name of the actor."
        },
        "foliageType": {
          "type": "string"
        },
        "foliageTypePath": {
          "type": "string",
          "description": "Asset path (e.g., /Game/Path/Asset)."
        },
        "meshPath": {
          "type": "string",
          "description": "Mesh asset path."
        },
        "density": {
          "type": "number"
        },
        "minScale": {
          "type": "number"
        },
        "maxScale": {
          "type": "number"
        },
        "cullDistance": {
          "type": "number"
        },
        "alignToNormal": {
          "type": "boolean"
        },
        "randomYaw": {
          "type": "boolean"
        },
        "locations": {
          "type": "array",
          "items": {
            "type": "object"
          }
        },
        "transforms": {
          "type": "array",
          "items": {
            "type": "object"
          }
        },
        "position": {
          "type": "object",
          "description": "3D location (x, y, z).",
          "properties": {
            "x": {
              "type": "number"
            },
            "y": {
              "type": "number"
            },
            "z": {
              "type": "number"
            }
          }
        },
        "bounds": {
          "type": "object",
          "additionalProperties": true
        },
        "volumeName": {
          "type": "string"
        },
        "seed": {
          "type": "number"
        },
        "foliageTypes": {
          "type": "array",
          "items": {
            "type": "object"
          }
        },
        "quadsPerSection": {
          "type": "number"
        },
        "enableWorldPartition": {
          "type": "boolean"
        },
        "runtimeGrid": {
          "type": "string"
        },
        "isSpatiallyLoaded": {
          "type": "boolean"
        },
        "dataLayers": {
          "type": "array",
          "items": {
            "type": "string"
          }
        },
        "count": {
          "type": "number"
        },
        "assets": {
          "type": "array",
          "items": {
            "type": "string"
          }
        },
        "numLODs": {
          "type": "number"
        },
        "subdivisions": {
          "type": "number"
        },
        "settings": {
          "type": "object",
          "additionalProperties": true
        },
        "tileSize": {
          "type": "number"
        },
        "quality": {
          "type": "string"
        },
        "staticMesh": {
          "type": "string",
          "description": "Mesh asset path."
        },
        "timeoutMs": {
          "type": "number"
        },
        "path": {
          "type": "string",
          "description": "Path to a directory."
        },
        "filename": {
          "type": "string"
        },
        "assetPaths": {
          "type": "array",
          "items": {
            "type": "string"
          }
        },
        "names": {
          "type": "array",
          "items": {
            "type": "string"
          }
        },
        "time": {
          "type": "number"
        },
        "spacing": {
          "type": "number"
        },
        "heightScale": {
          "type": "number"
        },
        "material": {
          "type": "string",
          "description": "Material asset path."
        },
        "hour": {
          "type": "number"
        },
        "intensity": {
          "type": "number"
        }
      },
      "required": [
        "action"
      ]
    }
  },
  {
    "name": "control_actor",
    "description": "Spawn actors, set transforms, enable physics, add components, manage tags, and attach actors.",
    "category": "core",
    "inputSchema": {
      "type": "object",
      "properties": {
        "action": {
          "type": "string",
          "description": "Action",
          "enum": [
            "spawn",
            "spawn_actor",
            "spawn_blueprint",
            "delete",
            "destroy_actor",
            "delete_by_tag",
            "duplicate",
            "apply_force",
            "set_transform",
            "teleport_actor",
            "set_actor_location",
            "set_actor_rotation",
            "set_actor_scale",
            "set_actor_transform",
            "get_transform",
            "get_actor_transform",
            "set_visibility",
            "set_actor_visible",
            "add_component",
            "remove_component",
            "set_component_properties",
            "set_component_property",
            "get_component_property",
            "get_components",
            "get_actor_components",
            "get_actor_bounds",
            "add_tag",
            "remove_tag",
            "find_by_tag",
            "find_actors_by_tag",
            "find_by_name",
            "find_actors_by_name",
            "find_by_class",
            "find_actors_by_class",
            "list",
            "set_blueprint_variables",
            "create_snapshot",
            "attach",
            "attach_actor",
            "detach",
            "detach_actor",
            "set_actor_collision",
            "call_actor_function"
          ]
        },
        "actorName": {
          "type": "string",
          "description": "Name of the actor."
        },
        "childActor": {
          "type": "string",
          "description": "Name of the child actor (for attach/detach operations)."
        },
        "parentActor": {
          "type": "string",
          "description": "Name of the parent actor (for attach operations)."
        },
        "classPath": {
          "type": "string",
          "description": "Asset path (e.g., /Game/Path/Asset)."
        },
        "meshPath": {
          "type": "string",
          "description": "Mesh asset path."
        },
        "blueprintPath": {
          "type": "string",
          "description": "Blueprint asset path."
        },
        "location": {
          "type": "object",
          "description": "3D location (x, y, z).",
          "properties": {
            "x": {
              "type": "number"
            },
            "y": {
              "type": "number"
            },
            "z": {
              "type": "number"
            }
          }
        },
        "rotation": {
          "type": "object",
          "description": "3D rotation (pitch, yaw, roll).",
          "properties": {
            "pitch": {
              "type": "number"
            },
            "yaw": {
              "type": "number"
            },
            "roll": {
              "type": "number"
            }
          }
        },
        "scale": {
          "type": "object",
          "description": "3D scale (x, y, z).",
          "properties": {
            "x": {
              "type": "number"
            },
            "y": {
              "type": "number"
            },
            "z": {
              "type": "number"
            }
          }
        },
        "force": {
          "type": "object",
          "description": "3D vector.",
          "properties": {
            "x": {
              "type": "number"
            },
            "y": {
              "type": "number"
            },
            "z": {
              "type": "number"
            }
          }
        },
        "componentType": {
          "type": "string"
        },
        "componentName": {
          "type": "string",
          "description": "Name of the component."
        },
        "properties": {
          "type": "object",
          "additionalProperties": true
        },
        "visible": {
          "type": "boolean",
          "description": "Whether the item/actor is visible."
        },
        "newName": {
          "type": "string",
          "description": "New name for renaming."
        },
        "tag": {
          "type": "string",
          "description": "Name of the tag."
        },
        "variables": {
          "type": "object",
          "additionalProperties": true
        },
        "snapshotName": {
          "type": "string"
        }
      },
      "required": [
        "action"
      ]
    }
  },
  {
    "name": "control_editor",
    "description": "Start/stop PIE, control viewport camera, run console commands, take screenshots, simulate input.",
    "category": "core",
    "inputSchema": {
      "type": "object",
      "properties": {
        "action": {
          "type": "string",
          "description": "Editor action. Note: screenshot/take_screenshot is async — the file is written on the next rendered viewport frame, not immediately. The editor window must be visible and actively rendering for capture to complete.",
          "enum": [
            "play",
            "stop",
            "stop_pie",
            "pause",
            "resume",
            "eject",
            "possess",
            "set_game_speed",
            "set_fixed_delta_time",
            "set_camera",
            "set_camera_position",
            "set_viewport_camera",
            "set_camera_fov",
            "set_view_mode",
            "set_viewport_resolution",
            "console_command",
            "execute_command",
            "screenshot",
            "take_screenshot",
            "step_frame",
            "single_frame_step",
            "start_recording",
            "stop_recording",
            "create_bookmark",
            "jump_to_bookmark",
            "set_preferences",
            "set_viewport_realtime",
            "open_asset",
            "close_asset",
            "simulate_input",
            "open_level",
            "focus_actor",
            "show_stats",
            "hide_stats",
            "set_editor_mode",
            "set_immersive_mode",
            "set_game_view",
            "undo",
            "redo",
            "save_all"
          ]
        },
        "location": {
          "type": "object",
          "description": "3D location (x, y, z).",
          "properties": {
            "x": {
              "type": "number"
            },
            "y": {
              "type": "number"
            },
            "z": {
              "type": "number"
            }
          }
        },
        "rotation": {
          "type": "object",
          "description": "3D rotation (pitch, yaw, roll).",
          "properties": {
            "pitch": {
              "type": "number"
            },
            "yaw": {
              "type": "number"
            },
            "roll": {
              "type": "number"
            }
          }
        },
        "viewMode": {
          "type": "string"
        },
        "enabled": {
          "type": "boolean",
          "description": "Whether the item/feature is enabled."
        },
        "speed": {
          "type": "number"
        },
        "filename": {
          "type": "string"
        },
        "fov": {
          "type": "number"
        },
        "width": {
          "type": "number"
        },
        "height": {
          "type": "number"
        },
        "command": {
          "type": "string"
        },
        "steps": {
          "type": "integer"
        },
        "bookmarkName": {
          "type": "string"
        },
        "assetPath": {
          "type": "string",
          "description": "Asset path (e.g., /Game/Path/Asset)."
        },
        "levelPath": {
          "type": "string",
          "description": "Level asset path."
        },
        "path": {
          "type": "string",
          "description": "Path to a directory."
        },
        "actorName": {
          "type": "string",
          "description": "Name of the actor."
        },
        "name": {
          "type": "string",
          "description": "Name identifier."
        },
        "mode": {
          "type": "string"
        },
        "deltaTime": {
          "type": "number"
        },
        "resolution": {
          "type": "string",
          "description": "Resolution setting (e.g., 1024x1024)."
        },
        "realtime": {
          "type": "boolean"
        },
        "stat": {
          "type": "string"
        },
        "category": {
          "type": "string"
        },
        "preferences": {
          "type": "object",
          "additionalProperties": true
        },
        "section": {
          "type": "string"
        },
        "key": {
          "type": "string"
        },
        "value": {
          "type": "object",
          "description": "Generic value (any type).",
          "additionalProperties": true
        },
        "inputAction": {
          "type": "string"
        },
        "axis": {
          "type": "string"
        }
      },
      "required": [
        "action"
      ]
    }
  },
  {
    "name": "inspect",
    "description": "Inspect any UObject: read/write properties, list components, export snapshots, and query class info. Actions: inspect_cdo (Blueprint CDO properties + all components without spawning an actor; use blueprintPath, optional detailed/componentName/propertyNames), inspect_class (class metadata), inspect_object (world actor), get_property/set_property, get_components, list_objects, find_by_class, find_by_tag, runtime_report.",
    "category": "core",
    "inputSchema": {
      "type": "object",
      "properties": {
        "action": {
          "type": "string",
          "description": "Action",
          "enum": [
            "inspect_object",
            "get_actor_details",
            "get_blueprint_details",
            "get_mesh_details",
            "get_texture_details",
            "get_material_details",
            "get_level_details",
            "get_component_details",
            "set_property",
            "get_property",
            "get_components",
            "get_component_property",
            "set_component_property",
            "inspect_class",
            "inspect_cdo",
            "runtime_report",
            "pie_report",
            "list_objects",
            "get_metadata",
            "add_tag",
            "find_by_tag",
            "create_snapshot",
            "restore_snapshot",
            "export",
            "delete_object",
            "find_by_class",
            "get_bounding_box",
            "get_project_settings",
            "get_world_settings",
            "get_viewport_info",
            "get_selected_actors",
            "get_scene_stats",
            "get_performance_stats",
            "get_memory_stats",
            "get_editor_settings"
          ]
        },
        "objectPath": {
          "type": "string",
          "description": "Asset path (e.g., /Game/Path/Asset)."
        },
        "propertyName": {
          "type": "string",
          "description": "Name of the property."
        },
        "propertyPath": {
          "type": "string",
          "description": "Dot-separated path into a struct or nested property (e.g., 'Transform.Location.X'). Use when 'propertyName' alone cannot address the target."
        },
        "value": {
          "type": "object",
          "description": "New value for set_property / set_component_property. Type matches the target property: number, string, boolean, array, or object.",
          "additionalProperties": true
        },
        "actorName": {
          "type": "string",
          "description": "Name of the actor."
        },
        "name": {
          "type": "string",
          "description": "Name identifier."
        },
        "componentName": {
          "type": "string",
          "description": "Name of the component."
        },
        "className": {
          "type": "string",
          "description": "UClass name (without /Script/ prefix) for inspect_class / inspect_cdo / find_by_class (e.g., 'StaticMeshActor')."
        },
        "classPath": {
          "type": "string",
          "description": "Asset path (e.g., /Game/Path/Asset)."
        },
        "tag": {
          "type": "string",
          "description": "Name of the tag."
        },
        "filter": {
          "type": "string",
          "description": "Substring or wildcard filter applied to object names for list_objects / find_by_class."
        },
        "snapshotName": {
          "type": "string",
          "description": "Identifier for create_snapshot / restore_snapshot. Snapshots capture an object's serialized state."
        },
        "destinationPath": {
          "type": "string",
          "description": "Destination path for move/copy."
        },
        "outputPath": {
          "type": "string",
          "description": "Output file or directory path."
        },
        "format": {
          "type": "string",
          "description": "Output format for export action: 'json' (default), 'text', or 'csv' depending on target."
        },
        "blueprintPath": {
          "type": "string",
          "description": "Blueprint asset path."
        },
        "detailed": {
          "type": "boolean",
          "description": "When true, *_details actions return extended property readback (component sub-properties, transient fields, etc.). Defaults to false."
        },
        "propertyNames": {
          "type": "array",
          "description": "Whitelist of property names to read in batch via get_property. Empty means all readable properties.",
          "items": {
            "type": "string"
          }
        },
        "componentNames": {
          "type": "array",
          "description": "Component names to include detailed property readback for.",
          "items": {
            "type": "string"
          }
        }
      },
      "required": [
        "action"
      ]
    }
  },
  {
    "name": "manage_ai",
    "description": "Create AI Controllers, configure Behavior Trees, Blackboards, EQS queries, and perception systems.",
    "category": "gameplay",
    "inputSchema": {
      "type": "object",
      "properties": {
        "action": {
          "type": "string",
          "description": "AI action to perform",
          "enum": [
            "create_ai_controller",
            "assign_behavior_tree",
            "assign_blackboard",
            "create_blackboard_asset",
            "add_blackboard_key",
            "set_key_instance_synced",
            "create_behavior_tree",
            "add_composite_node",
            "add_task_node",
            "add_decorator",
            "add_service",
            "configure_bt_node",
            "create_eqs_query",
            "add_eqs_generator",
            "add_eqs_context",
            "add_eqs_test",
            "configure_test_scoring",
            "add_ai_perception_component",
            "configure_sight_config",
            "configure_hearing_config",
            "configure_damage_sense_config",
            "set_perception_team",
            "create_state_tree",
            "add_state_tree_state",
            "add_state_tree_transition",
            "configure_state_tree_task",
            "create_smart_object_definition",
            "add_smart_object_slot",
            "configure_slot_behavior",
            "add_smart_object_component",
            "create_mass_entity_config",
            "configure_mass_entity",
            "add_mass_spawner",
            "get_ai_info",
            "create_blackboard",
            "setup_perception",
            "create_nav_link_proxy",
            "set_focus",
            "clear_focus",
            "set_blackboard_value",
            "get_blackboard_value",
            "run_behavior_tree",
            "stop_behavior_tree"
          ]
        },
        "name": {
          "type": "string",
          "description": "Name identifier."
        },
        "path": {
          "type": "string",
          "description": "Directory path for asset creation."
        },
        "blueprintPath": {
          "type": "string",
          "description": "Blueprint asset path."
        },
        "controllerPath": {
          "type": "string",
          "description": "Path to controller blueprint."
        },
        "behaviorTreePath": {
          "type": "string",
          "description": "Path to behavior tree asset."
        },
        "blackboardPath": {
          "type": "string",
          "description": "Path to blackboard asset."
        },
        "parentClass": {
          "type": "string",
          "description": "Parent class for AI controller (default: AAIController).",
          "enum": [
            "AAIController",
            "APlayerController"
          ]
        },
        "autoRunBehaviorTree": {
          "type": "boolean",
          "description": "Start behavior tree automatically on possess."
        },
        "keyName": {
          "type": "string",
          "description": "Name of the key."
        },
        "keyType": {
          "type": "string",
          "description": "Blackboard key data type.",
          "enum": [
            "Bool",
            "Int",
            "Float",
            "Vector",
            "Rotator",
            "Object",
            "Class",
            "Enum",
            "Name",
            "String"
          ]
        },
        "isInstanceSynced": {
          "type": "boolean",
          "description": "Sync key across instances."
        },
        "baseObjectClass": {
          "type": "string",
          "description": "Base class for Object/Class keys."
        },
        "enumClass": {
          "type": "string",
          "description": "Enum class for Enum keys."
        },
        "compositeType": {
          "type": "string",
          "description": "Composite node type.",
          "enum": [
            "Selector",
            "Sequence",
            "Parallel",
            "SimpleParallel"
          ]
        },
        "taskType": {
          "type": "string",
          "description": "Task node type.",
          "enum": [
            "MoveTo",
            "MoveDirectlyToward",
            "RotateToFaceBBEntry",
            "Wait",
            "WaitBlackboardTime",
            "PlayAnimation",
            "PlaySound",
            "RunEQSQuery",
            "RunBehaviorDynamic",
            "SetBlackboardValue",
            "PushPawnAction",
            "FinishWithResult",
            "MakeNoise",
            "GameplayTaskBase",
            "Custom"
          ]
        },
        "decoratorType": {
          "type": "string",
          "description": "Decorator node type.",
          "enum": [
            "Blackboard",
            "BlackboardBased",
            "CompareBBEntries",
            "Cooldown",
            "ConeCheck",
            "DoesPathExist",
            "IsAtLocation",
            "IsBBEntryOfClass",
            "KeepInCone",
            "Loop",
            "SetTagCooldown",
            "TagCooldown",
            "TimeLimit",
            "ForceSuccess",
            "ConditionalLoop",
            "Custom"
          ]
        },
        "serviceType": {
          "type": "string",
          "description": "Service node type.",
          "enum": [
            "DefaultFocus",
            "RunEQS",
            "Custom"
          ]
        },
        "parentNodeId": {
          "type": "string",
          "description": "ID of the node."
        },
        "nodeId": {
          "type": "string",
          "description": "ID of the node."
        },
        "nodeProperties": {
          "type": "object",
          "description": "Properties to set on the node.",
          "additionalProperties": true
        },
        "customTaskClass": {
          "type": "string",
          "description": "Custom task class path for Custom task type."
        },
        "customDecoratorClass": {
          "type": "string",
          "description": "Custom decorator class path."
        },
        "customServiceClass": {
          "type": "string",
          "description": "Custom service class path."
        },
        "queryPath": {
          "type": "string",
          "description": "Path to EQS query asset."
        },
        "generatorType": {
          "type": "string",
          "description": "EQS generator type.",
          "enum": [
            "ActorsOfClass",
            "CurrentLocation",
            "Donut",
            "OnCircle",
            "PathingGrid",
            "SimpleGrid",
            "Composite",
            "Custom"
          ]
        },
        "contextType": {
          "type": "string",
          "description": "EQS context type.",
          "enum": [
            "Querier",
            "Item",
            "EnvQueryContext_BlueprintBase",
            "Custom"
          ]
        },
        "testType": {
          "type": "string",
          "description": "EQS test type.",
          "enum": [
            "Distance",
            "Dot",
            "GameplayTags",
            "Overlap",
            "Pathfinding",
            "PathfindingBatch",
            "Project",
            "Random",
            "Trace",
            "Custom"
          ]
        },
        "generatorSettings": {
          "type": "object",
          "description": "Generator-specific settings.",
          "properties": {
            "searchRadius": {
              "type": "number"
            },
            "searchCenter": {
              "type": "string"
            },
            "actorClass": {
              "type": "string"
            },
            "gridSize": {
              "type": "number"
            },
            "spacesBetween": {
              "type": "number"
            },
            "innerRadius": {
              "type": "number"
            },
            "outerRadius": {
              "type": "number"
            }
          }
        },
        "testSettings": {
          "type": "object",
          "description": "Test scoring and filter settings.",
          "properties": {
            "scoringEquation": {
              "type": "string",
              "enum": [
                "Linear",
                "Square",
                "InverseLinear",
                "Constant"
              ]
            },
            "clampMin": {
              "type": "number"
            },
            "clampMax": {
              "type": "number"
            },
            "filterType": {
              "type": "string",
              "enum": [
                "Minimum",
                "Maximum",
                "Range"
              ]
            },
            "floatMin": {
              "type": "number"
            },
            "floatMax": {
              "type": "number"
            }
          }
        },
        "testIndex": {
          "type": "number",
          "description": "Index of test to configure."
        },
        "sightConfig": {
          "type": "object",
          "description": "AI sight sense configuration.",
          "properties": {
            "sightRadius": {
              "type": "number"
            },
            "loseSightRadius": {
              "type": "number"
            },
            "peripheralVisionAngle": {
              "type": "number"
            },
            "pointOfViewBackwardOffset": {
              "type": "number"
            },
            "nearClippingRadius": {
              "type": "number"
            },
            "autoSuccessRange": {
              "type": "number"
            },
            "maxAge": {
              "type": "number"
            },
            "detectionByAffiliation": {
              "type": "object",
              "properties": {
                "enemies": {
                  "type": "boolean"
                },
                "neutrals": {
                  "type": "boolean"
                },
                "friendlies": {
                  "type": "boolean"
                }
              }
            }
          }
        },
        "hearingConfig": {
          "type": "object",
          "description": "AI hearing sense configuration.",
          "properties": {
            "hearingRange": {
              "type": "number"
            },
            "loSHearingRange": {
              "type": "number"
            },
            "detectFriendly": {
              "type": "boolean"
            },
            "maxAge": {
              "type": "number"
            }
          }
        },
        "damageConfig": {
          "type": "object",
          "description": "AI damage sense configuration.",
          "properties": {
            "maxAge": {
              "type": "number"
            }
          }
        },
        "teamId": {
          "type": "number",
          "description": "Team ID for perception affiliation (0=Neutral, 1=Player, 2=Enemy, etc.)."
        },
        "dominantSense": {
          "type": "string",
          "description": "Dominant sense for perception prioritization.",
          "enum": [
            "Sight",
            "Hearing",
            "Damage",
            "Touch",
            "None"
          ]
        },
        "stateTreePath": {
          "type": "string",
          "description": "Path to State Tree asset."
        },
        "stateName": {
          "type": "string",
          "description": "Name of the state."
        },
        "fromState": {
          "type": "string",
          "description": "Source state name."
        },
        "toState": {
          "type": "string",
          "description": "Target state name."
        },
        "transitionCondition": {
          "type": "string",
          "description": "Condition expression for transition."
        },
        "stateTaskClass": {
          "type": "string",
          "description": "Task class for state."
        },
        "stateEvaluatorClass": {
          "type": "string",
          "description": "Evaluator class for state."
        },
        "definitionPath": {
          "type": "string",
          "description": "Path to definition asset."
        },
        "slotIndex": {
          "type": "number",
          "description": "Index of slot to configure."
        },
        "slotOffset": {
          "type": "object",
          "description": "Local offset for slot.",
          "properties": {
            "x": {
              "type": "number"
            },
            "y": {
              "type": "number"
            },
            "z": {
              "type": "number"
            }
          }
        },
        "slotRotation": {
          "type": "object",
          "description": "Local rotation for slot.",
          "properties": {
            "pitch": {
              "type": "number"
            },
            "yaw": {
              "type": "number"
            },
            "roll": {
              "type": "number"
            }
          }
        },
        "slotBehaviorDefinition": {
          "type": "string",
          "description": "Gameplay behavior definition for slot."
        },
        "slotActivityTags": {
          "type": "array",
          "description": "Activity tags for the slot.",
          "items": {
            "type": "string"
          }
        },
        "slotUserTags": {
          "type": "array",
          "description": "Required user tags for slot.",
          "items": {
            "type": "string"
          }
        },
        "slotEnabled": {
          "type": "boolean",
          "description": "Whether slot is enabled."
        },
        "configPath": {
          "type": "string",
          "description": "Path to config asset."
        },
        "massTraits": {
          "type": "array",
          "description": "List of Mass traits to add.",
          "items": {
            "type": "string"
          }
        },
        "massProcessors": {
          "type": "array",
          "description": "List of Mass processors to configure.",
          "items": {
            "type": "string"
          }
        },
        "spawnerSettings": {
          "type": "object",
          "description": "Mass spawner configuration.",
          "properties": {
            "entityCount": {
              "type": "number"
            },
            "spawnRadius": {
              "type": "number"
            },
            "entityConfig": {
              "type": "string"
            },
            "spawnOnBeginPlay": {
              "type": "boolean"
            }
          }
        },
        "value": {
          "type": "object",
          "description": "Generic value (any type).",
          "additionalProperties": true
        }
      },
      "required": [
        "action"
      ]
    }
  },
  {
    "name": "manage_asset",
    "description": "Create, import, duplicate, rename, delete assets. Edit Material graphs and instances. Analyze dependencies.",
    "category": "core",
    "inputSchema": {
      "type": "object",
      "properties": {
        "action": {
          "type": "string",
          "description": "Action to perform",
          "enum": [
            "list",
            "import",
            "duplicate",
            "duplicate_asset",
            "rename",
            "rename_asset",
            "move",
            "move_asset",
            "delete",
            "delete_asset",
            "delete_assets",
            "create_folder",
            "search_assets",
            "get_dependencies",
            "get_source_control_state",
            "analyze_graph",
            "get_asset_graph",
            "create_thumbnail",
            "set_tags",
            "get_metadata",
            "set_metadata",
            "validate",
            "fixup_redirectors",
            "find_by_tag",
            "generate_report",
            "create_material",
            "create_material_instance",
            "create_render_target",
            "generate_lods",
            "add_material_parameter",
            "list_instances",
            "reset_instance_parameters",
            "exists",
            "get_material_stats",
            "get_material_instance_info",
            "find_material_expressions",
            "get_material_expression_details",
            "get_material_expression_connections",
            "get_landscape_material_context",
            "compile_material_diagnostics",
            "nanite_rebuild_mesh",
            "bulk_rename",
            "bulk_delete",
            "source_control_checkout",
            "source_control_submit",
            "add_material_node",
            "set_material_node_position",
            "move_material_node",
            "bulk_set_material_node_positions",
            "bulk_move_material_nodes",
            "connect_material_pins",
            "remove_material_node",
            "break_material_connections",
            "create_material_comment",
            "wrap_material_nodes_in_comment",
            "create_named_reroute",
            "use_named_reroute",
            "replace_long_connection_with_named_reroute",
            "align_material_nodes",
            "get_material_node_details",
            "rebuild_material",
            "get_set_material_attributes_overrides"
          ]
        },
        "assetPath": {
          "type": "string",
          "description": "Asset path (e.g., /Game/Path/Asset)."
        },
        "directory": {
          "type": "string",
          "description": "Path to a directory."
        },
        "classNames": {
          "type": "array",
          "description": "UClass names to filter by (e.g., 'Material', 'StaticMesh') for list / search_assets.",
          "items": {
            "type": "string"
          }
        },
        "packagePaths": {
          "type": "array",
          "description": "Package paths to scope list / search_assets. Empty means all paths.",
          "items": {
            "type": "string"
          }
        },
        "recursivePaths": {
          "type": "boolean",
          "description": "When true, list / search_assets descends into subdirectories of packagePaths."
        },
        "recursiveClasses": {
          "type": "boolean",
          "description": "When true, list / search_assets includes subclasses of classNames."
        },
        "limit": {
          "type": "number",
          "description": "Maximum number of results to return (paginated list / search_assets)."
        },
        "offset": {
          "type": "number",
          "description": "Skip this many results before returning (paginated list / search_assets)."
        },
        "sourcePath": {
          "type": "string",
          "description": "Source path for import/move/copy."
        },
        "destinationPath": {
          "type": "string",
          "description": "Destination path for move/copy."
        },
        "assetPaths": {
          "type": "array",
          "description": "List of asset paths for batch operations (delete_assets, bulk_rename, bulk_delete, source_control_submit, etc.).",
          "items": {
            "type": "string"
          }
        },
        "lodCount": {
          "type": "number",
          "description": "Number of LODs to generate via generate_lods."
        },
        "reductionSettings": {
          "type": "object",
          "description": "Per-LOD mesh reduction settings for generate_lods (e.g., screen size, percent triangles).",
          "additionalProperties": true
        },
        "nodeName": {
          "type": "string",
          "description": "Name identifier."
        },
        "eventName": {
          "type": "string",
          "description": "Name of the event."
        },
        "memberClass": {
          "type": "string",
          "description": "Variable/member class name (e.g., 'StaticMeshComponent') when adding parameters or member entries via authoring actions."
        },
        "posX": {
          "type": "number",
          "description": "Material graph X target absolute position for set_material_node_position / move_material_node."
        },
        "posY": {
          "type": "number",
          "description": "Material graph Y target absolute position for set_material_node_position / move_material_node."
        },
        "newName": {
          "type": "string",
          "description": "New name for renaming."
        },
        "overwrite": {
          "type": "boolean",
          "description": "Overwrite if the asset/file already exists."
        },
        "save": {
          "type": "boolean",
          "description": "Save the asset(s) after the operation."
        },
        "fixupRedirectors": {
          "type": "boolean",
          "description": "After move/rename, run fixup-redirectors to consolidate references and delete redirector stubs."
        },
        "directoryPath": {
          "type": "string",
          "description": "Path to a directory."
        },
        "name": {
          "type": "string",
          "description": "Name identifier."
        },
        "path": {
          "type": "string",
          "description": "Path to a directory."
        },
        "parentMaterial": {
          "type": "string",
          "description": "Material asset path."
        },
        "parameters": {
          "type": "object",
          "description": "Parameter overrides for create_material_instance: key/value pairs whose value shape matches the parameter type (number for scalar, {r,g,b,a} for vector, asset path for texture, bool for switch).",
          "additionalProperties": true
        },
        "width": {
          "type": "number",
          "description": "Width in pixels for create_thumbnail / create_render_target."
        },
        "height": {
          "type": "number",
          "description": "Height in pixels for create_thumbnail / create_render_target."
        },
        "format": {
          "type": "string",
          "description": "Format identifier: pixel format (e.g., 'RGBA16f') for create_render_target, output format ('png'/'json'/'csv') for export/report actions."
        },
        "meshPath": {
          "type": "string",
          "description": "Mesh asset path."
        },
        "tag": {
          "type": "string",
          "description": "Name of the tag."
        },
        "metadata": {
          "type": "object",
          "description": "Key-value metadata pairs to attach via set_metadata.",
          "additionalProperties": true
        },
        "graphName": {
          "type": "string",
          "description": "Name of the graph."
        },
        "nodeType": {
          "type": "string",
          "description": "Material node type identifier for add_material_node (e.g., 'MaterialExpressionConstant', 'TextureSampleParameter2D')."
        },
        "nodeId": {
          "type": "string",
          "description": "ID of the node."
        },
        "expressionPath": {
          "type": "string",
          "description": "Material expression path."
        },
        "expressionName": {
          "type": "string",
          "description": "Material expression object name."
        },
        "expressionGuid": {
          "type": "string",
          "description": "Material expression GUID."
        },
        "className": {
          "type": "string",
          "description": "Material expression class name."
        },
        "sourceExpressionIndex": {
          "type": "number",
          "description": "Source expression index."
        },
        "targetExpressionIndex": {
          "type": "number",
          "description": "Target expression index."
        },
        "sourceExpressionPath": {
          "type": "string",
          "description": "Source expression path."
        },
        "targetExpressionPath": {
          "type": "string",
          "description": "Target expression path."
        },
        "anchorExpressionPath": {
          "type": "string",
          "description": "Anchor expression path."
        },
        "anchorExpressionIndex": {
          "type": "number",
          "description": "Anchor expression index."
        },
        "anchorNodeId": {
          "type": "string",
          "description": "Anchor node ID."
        },
        "sourceNodeId": {
          "type": "string",
          "description": "ID of the source node."
        },
        "targetNodeId": {
          "type": "string",
          "description": "ID of the target node."
        },
        "sourceOutputIndex": {
          "type": "number",
          "description": "Source output index."
        },
        "inputName": {
          "type": "string",
          "description": "Name of the pin."
        },
        "targetInputPin": {
          "type": "string",
          "description": "Name of the target input pin."
        },
        "fromNodeId": {
          "type": "string",
          "description": "ID of the source node."
        },
        "fromPin": {
          "type": "string",
          "description": "Name of the source pin."
        },
        "toNodeId": {
          "type": "string",
          "description": "ID of the target node."
        },
        "toPin": {
          "type": "string",
          "description": "Name of the target pin."
        },
        "parameterName": {
          "type": "string",
          "description": "Name of the parameter."
        },
        "value": {
          "type": "object",
          "description": "Generic value (any type).",
          "additionalProperties": true
        },
        "x": {
          "type": "number",
          "description": "Material graph X position for add_material_node placement (use posX for set_material_node_position / move_material_node)."
        },
        "y": {
          "type": "number",
          "description": "Material graph Y position for add_material_node placement (use posY for set_material_node_position / move_material_node)."
        },
        "comment": {
          "type": "string",
          "description": "Comment text for create_material_comment / wrap_material_nodes_in_comment."
        },
        "operation": {
          "type": "string",
          "description": "Alignment or distribution operation."
        },
        "backend": {
          "type": "string",
          "description": "native or graph_editor."
        },
        "placementMode": {
          "type": "string",
          "description": "absolute, next_to, or free."
        },
        "direction": {
          "type": "string",
          "description": "right, left, above, or below."
        },
        "avoidOverlap": {
          "type": "boolean",
          "description": "Avoid overlapping existing material nodes."
        },
        "placement": {
          "type": "object",
          "description": "Material node placement options.",
          "additionalProperties": true
        },
        "padding": {
          "type": "number",
          "description": "Padding for comment wrapping."
        },
        "groupMode": {
          "type": "boolean",
          "description": "Whether comment moves grouped nodes."
        },
        "minDistance": {
          "type": "number",
          "description": "Minimum distance for long connection replacement."
        },
        "declarationId": {
          "type": "string",
          "description": "Named reroute declaration ID."
        },
        "declarationGuid": {
          "type": "string",
          "description": "Named reroute declaration GUID."
        },
        "declarationName": {
          "type": "string",
          "description": "Named reroute declaration name."
        },
        "parentNodeId": {
          "type": "string",
          "description": "ID of the node."
        },
        "childNodeId": {
          "type": "string",
          "description": "ID of the node."
        },
        "maxDepth": {
          "type": "number",
          "description": "Maximum recursion depth for get_dependencies / analyze_graph. Zero or unset means unlimited."
        },
        "prefix": {
          "type": "string",
          "description": "Prefix to prepend to asset names during bulk_rename."
        },
        "suffix": {
          "type": "string",
          "description": "Suffix to append to asset names during bulk_rename."
        },
        "searchText": {
          "type": "string",
          "description": "Substring to find in asset names during bulk_rename."
        },
        "replaceText": {
          "type": "string",
          "description": "Replacement text for searchText during bulk_rename."
        },
        "paths": {
          "type": "array",
          "description": "List of paths for bulk operations like fixup_redirectors and source_control_submit.",
          "items": {
            "type": "string"
          }
        },
        "description": {
          "type": "string",
          "description": "Free-text description body. For set_metadata, the human-readable description value; for material comments, the comment text body. Distinct from the short label 'desc'."
        },
        "checkoutFiles": {
          "type": "boolean",
          "description": "When true, automatically check out files from source control before edit/delete operations."
        },
        "showConfirmation": {
          "type": "boolean",
          "description": "When true, show modal confirmation dialogs (default false to keep automation headless)."
        },
        "pinName": {
          "type": "string",
          "description": "Name of the pin."
        },
        "desc": {
          "type": "string",
          "description": "Short label/identifier (e.g., named-reroute display name, report short title). Distinct from the longer free-text 'description'."
        },
        "materialPath": {
          "type": "string",
          "description": "Material asset path."
        },
        "texturePath": {
          "type": "string",
          "description": "Texture asset path."
        },
        "expressionClass": {
          "type": "string",
          "description": "Fully-qualified material expression class name for add_material_node (e.g., 'MaterialExpressionMultiply'). Same role as 'className' on material-graph actions."
        },
        "coordinateIndex": {
          "type": "number",
          "description": "UV coordinate index (0-based) for texture-sample expression nodes."
        },
        "parameterType": {
          "type": "string",
          "description": "Parameter type for add_material_parameter: 'scalar', 'vector', 'static_switch', 'texture_object', or 'texture_sample'."
        },
        "nodes": {
          "type": "array",
          "description": "Batch list of material node specs for bulk_set_material_node_positions / bulk_move_material_nodes / wrap_material_nodes_in_comment.",
          "items": {
            "type": "object"
          }
        },
        "tags": {
          "type": "array",
          "description": "Asset tag list (string array) for set_tags / find_by_tag. Use 'tag' (singular) for single-tag actions.",
          "items": {
            "type": "string"
          }
        },
        "actorName": {
          "type": "string",
          "description": "Actor name for landscape context diagnostics."
        },
        "actorPath": {
          "type": "string",
          "description": "Actor object path for landscape context diagnostics."
        },
        "landscapeName": {
          "type": "string",
          "description": "Landscape actor name."
        },
        "landscapePath": {
          "type": "string",
          "description": "Landscape actor object path."
        },
        "includeEffective": {
          "type": "boolean",
          "description": "Include effective inherited parameter values."
        },
        "overriddenOnly": {
          "type": "boolean",
          "description": "Restrict diagnostics to explicitly overridden parameters."
        },
        "includeConsumers": {
          "type": "boolean",
          "description": "Include downstream consumers in expression connection diagnostics."
        },
        "folderPath": {
          "type": "string",
          "description": "Path to a directory."
        },
        "sourceNode": {
          "type": "string",
          "description": "ID of the source node."
        },
        "targetNode": {
          "type": "string",
          "description": "ID of the target node."
        },
        "outputPin": {
          "type": "string",
          "description": "Name of the source pin."
        },
        "inputPin": {
          "type": "string",
          "description": "Name of the target pin."
        },
        "type": {
          "type": "string",
          "description": "Action-specific type discriminator. Prefer the more specific 'parameterType' / 'expressionClass' / 'nodeType' when the action exposes them; this generic 'type' exists only for actions that have not yet been migrated to a specific name."
        },
        "defaultValue": {
          "type": "object",
          "description": "Generic value (any type).",
          "additionalProperties": true
        },
        "expressionIndex": {
          "type": "number",
          "description": "Material expression index."
        }
      },
      "required": [
        "action"
      ]
    },
    "annotations": {
      "destructiveHint": true,
      "idempotentHint": false
    }
  },
  {
    "name": "manage_audio",
    "description": "Play/stop sounds, add audio components, configure mixes, attenuation, spatial audio, and author Sound Cues/MetaSounds.",
    "category": "utility",
    "inputSchema": {
      "type": "object",
      "properties": {
        "action": {
          "type": "string",
          "description": "Action",
          "enum": [
            "create_sound_cue",
            "play_sound_at_location",
            "play_sound_2d",
            "create_audio_component",
            "create_sound_mix",
            "push_sound_mix",
            "pop_sound_mix",
            "set_sound_mix_class_override",
            "clear_sound_mix_class_override",
            "set_base_sound_mix",
            "prime_sound",
            "play_sound_attached",
            "spawn_sound_at_location",
            "fade_sound_in",
            "fade_sound_out",
            "create_ambient_sound",
            "create_sound_class",
            "set_sound_attenuation",
            "create_reverb_zone",
            "enable_audio_analysis",
            "fade_sound",
            "set_doppler_effect",
            "set_audio_occlusion",
            "add_cue_node",
            "connect_cue_nodes",
            "set_cue_attenuation",
            "set_cue_concurrency",
            "create_metasound",
            "add_metasound_node",
            "connect_metasound_nodes",
            "add_metasound_input",
            "add_metasound_output",
            "set_metasound_default",
            "set_class_properties",
            "set_class_parent",
            "add_mix_modifier",
            "configure_mix_eq",
            "create_attenuation_settings",
            "configure_distance_attenuation",
            "configure_spatialization",
            "configure_occlusion",
            "configure_reverb_send",
            "create_dialogue_voice",
            "create_dialogue_wave",
            "set_dialogue_context",
            "create_reverb_effect",
            "create_source_effect_chain",
            "add_source_effect",
            "create_submix_effect",
            "get_audio_info"
          ]
        },
        "name": {
          "type": "string",
          "description": "Name identifier."
        },
        "soundPath": {
          "type": "string",
          "description": "Sound asset path."
        },
        "location": {
          "type": "object",
          "description": "3D location (x, y, z).",
          "properties": {
            "x": {
              "type": "number"
            },
            "y": {
              "type": "number"
            },
            "z": {
              "type": "number"
            }
          }
        },
        "rotation": {
          "type": "object",
          "description": "3D rotation (pitch, yaw, roll).",
          "properties": {
            "pitch": {
              "type": "number"
            },
            "yaw": {
              "type": "number"
            },
            "roll": {
              "type": "number"
            }
          }
        },
        "volume": {
          "type": "number"
        },
        "pitch": {
          "type": "number"
        },
        "startTime": {
          "type": "number"
        },
        "attenuationPath": {
          "type": "string",
          "description": "Asset path (e.g., /Game/Path/Asset)."
        },
        "concurrencyPath": {
          "type": "string",
          "description": "Asset path (e.g., /Game/Path/Asset)."
        },
        "mixName": {
          "type": "string"
        },
        "soundClassName": {
          "type": "string"
        },
        "fadeInTime": {
          "type": "number"
        },
        "fadeOutTime": {
          "type": "number"
        },
        "fadeTime": {
          "type": "number"
        },
        "targetVolume": {
          "type": "number"
        },
        "attachPointName": {
          "type": "string",
          "description": "Name of the socket."
        },
        "actorName": {
          "type": "string",
          "description": "Name of the actor."
        },
        "componentName": {
          "type": "string",
          "description": "Name of the component."
        },
        "parentClass": {
          "type": "string"
        },
        "properties": {
          "type": "object",
          "additionalProperties": true
        },
        "innerRadius": {
          "type": "number"
        },
        "falloffDistance": {
          "type": "number"
        },
        "attenuationShape": {
          "type": "string"
        },
        "falloffMode": {
          "type": "string"
        },
        "reverbEffect": {
          "type": "string"
        },
        "size": {
          "type": "object",
          "description": "3D scale (x, y, z).",
          "properties": {
            "x": {
              "type": "number"
            },
            "y": {
              "type": "number"
            },
            "z": {
              "type": "number"
            }
          }
        },
        "fftSize": {
          "type": "number"
        },
        "outputType": {
          "type": "string"
        },
        "soundName": {
          "type": "string"
        },
        "fadeType": {
          "type": "string"
        },
        "scale": {
          "type": "number"
        },
        "lowPassFilterFrequency": {
          "type": "number"
        },
        "volumeAttenuation": {
          "type": "number"
        },
        "enabled": {
          "type": "boolean",
          "description": "Whether the item/feature is enabled."
        },
        "path": {
          "type": "string",
          "description": "Directory path for asset creation."
        },
        "assetPath": {
          "type": "string",
          "description": "Asset path (e.g., /Game/Path/Asset)."
        },
        "save": {
          "type": "boolean",
          "description": "Save the asset(s) after the operation."
        },
        "wavePath": {
          "type": "string",
          "description": "Path to SoundWave asset."
        },
        "nodeType": {
          "type": "string"
        },
        "nodeId": {
          "type": "string",
          "description": "ID of the node."
        },
        "sourceNodeId": {
          "type": "string",
          "description": "ID of the source node."
        },
        "targetNodeId": {
          "type": "string",
          "description": "ID of the target node."
        },
        "outputPin": {
          "type": "number"
        },
        "inputPin": {
          "type": "number"
        },
        "looping": {
          "type": "boolean",
          "description": "Whether to loop."
        },
        "x": {
          "type": "number"
        },
        "y": {
          "type": "number"
        },
        "metasoundType": {
          "type": "string"
        },
        "inputName": {
          "type": "string",
          "description": "Name of the input."
        },
        "inputType": {
          "type": "string"
        },
        "outputName": {
          "type": "string",
          "description": "Name of the output."
        },
        "sourceNode": {
          "type": "string",
          "description": "Source node name."
        },
        "sourcePin": {
          "type": "string",
          "description": "Name of the source pin."
        },
        "targetNode": {
          "type": "string",
          "description": "Target node name."
        },
        "targetPin": {
          "type": "string",
          "description": "Name of the target pin."
        },
        "defaultValue": {
          "type": "object",
          "additionalProperties": true
        },
        "metasoundNodeType": {
          "type": "string"
        },
        "soundClassPath": {
          "type": "string",
          "description": "Sound class path."
        },
        "parentClassPath": {
          "type": "string",
          "description": "Parent class path."
        }
      },
      "required": [
        "action"
      ]
    }
  },
  {
    "name": "manage_behavior_tree",
    "description": "Create Behavior Trees, add task/decorator/service nodes, and configure node properties.",
    "category": "utility",
    "inputSchema": {
      "type": "object",
      "properties": {
        "action": {
          "type": "string",
          "description": "Action",
          "enum": [
            "create",
            "add_node",
            "connect_nodes",
            "remove_node",
            "break_connections",
            "set_node_properties"
          ]
        },
        "name": {
          "type": "string",
          "description": "Name identifier."
        },
        "savePath": {
          "type": "string",
          "description": "Path to save the asset."
        },
        "assetPath": {
          "type": "string",
          "description": "Asset path (e.g., /Game/Path/Asset)."
        },
        "nodeType": {
          "type": "string"
        },
        "nodeId": {
          "type": "string",
          "description": "ID of the node."
        },
        "parentNodeId": {
          "type": "string",
          "description": "ID of the node."
        },
        "childNodeId": {
          "type": "string",
          "description": "ID of the node."
        },
        "x": {
          "type": "number"
        },
        "y": {
          "type": "number"
        },
        "comment": {
          "type": "string"
        },
        "properties": {
          "type": "object",
          "additionalProperties": true
        }
      },
      "required": [
        "action"
      ]
    }
  },
  {
    "name": "manage_blueprint",
    "description": "Create Blueprints, add SCS components (mesh, collision, camera), and manipulate graph nodes.",
    "category": "authoring",
    "inputSchema": {
      "type": "object",
      "properties": {
        "action": {
          "type": "string",
          "description": "Blueprint action",
          "enum": [
            "create",
            "get_blueprint",
            "get",
            "compile",
            "add_component",
            "set_default",
            "modify_scs",
            "get_scs",
            "add_scs_component",
            "remove_scs_component",
            "reparent_scs_component",
            "set_scs_transform",
            "set_scs_property",
            "ensure_exists",
            "probe_handle",
            "add_variable",
            "remove_variable",
            "rename_variable",
            "add_function",
            "add_event",
            "remove_event",
            "add_construction_script",
            "set_variable_metadata",
            "set_metadata",
            "create_node",
            "add_node",
            "delete_node",
            "connect_pins",
            "break_pin_links",
            "set_node_property",
            "create_reroute_node",
            "get_node_details",
            "get_graph_details",
            "get_pin_details",
            "list_node_types",
            "set_pin_default_value"
          ]
        },
        "name": {
          "type": "string",
          "description": "Name identifier."
        },
        "blueprintPath": {
          "type": "string",
          "description": "Blueprint asset path."
        },
        "blueprintType": {
          "type": "string",
          "description": "Path or name of the parent class."
        },
        "savePath": {
          "type": "string",
          "description": "Path to save the asset."
        },
        "componentType": {
          "type": "string"
        },
        "componentName": {
          "type": "string",
          "description": "Name of the component."
        },
        "componentClass": {
          "type": "string"
        },
        "attachTo": {
          "type": "string"
        },
        "newParent": {
          "type": "string"
        },
        "propertyName": {
          "type": "string",
          "description": "Name of the property."
        },
        "variableName": {
          "type": "string",
          "description": "Name of the variable."
        },
        "oldName": {
          "type": "string"
        },
        "newName": {
          "type": "string",
          "description": "New name for renaming."
        },
        "value": {
          "type": "object",
          "description": "Generic value (any type).",
          "additionalProperties": true
        },
        "metadata": {
          "type": "object",
          "additionalProperties": true
        },
        "properties": {
          "type": "object",
          "additionalProperties": true
        },
        "graphName": {
          "type": "string",
          "description": "Name of the graph."
        },
        "nodeType": {
          "type": "string"
        },
        "nodeId": {
          "type": "string",
          "description": "ID of the node."
        },
        "pinName": {
          "type": "string",
          "description": "Name of the pin."
        },
        "linkedTo": {
          "type": "string"
        },
        "memberName": {
          "type": "string"
        },
        "x": {
          "type": "number"
        },
        "y": {
          "type": "number"
        },
        "location": {
          "type": "object",
          "description": "3D location (x, y, z).",
          "properties": {
            "x": {
              "type": "number",
              "description": "X coordinate."
            },
            "y": {
              "type": "number",
              "description": "Y coordinate."
            },
            "z": {
              "type": "number",
              "description": "Z coordinate."
            }
          },
          "required": [
            "x",
            "y",
            "z"
          ]
        },
        "rotation": {
          "type": "object",
          "description": "3D rotation (pitch, yaw, roll).",
          "properties": {
            "pitch": {
              "type": "number",
              "description": "Pitch."
            },
            "yaw": {
              "type": "number",
              "description": "Yaw."
            },
            "roll": {
              "type": "number",
              "description": "Roll."
            }
          },
          "required": [
            "pitch",
            "yaw",
            "roll"
          ]
        },
        "scale": {
          "type": "object",
          "description": "3D scale (x, y, z).",
          "properties": {
            "x": {
              "type": "number",
              "description": "X scale."
            },
            "y": {
              "type": "number",
              "description": "Y scale."
            },
            "z": {
              "type": "number",
              "description": "Z scale."
            }
          },
          "required": [
            "x",
            "y",
            "z"
          ]
        },
        "operations": {
          "type": "array",
          "items": {
            "type": "object"
          }
        },
        "compile": {
          "type": "boolean",
          "description": "Compile the blueprint(s) after the operation."
        },
        "save": {
          "type": "boolean",
          "description": "Save the asset(s) after the operation."
        },
        "eventType": {
          "type": "string"
        },
        "customEventName": {
          "type": "string",
          "description": "Name of the event."
        },
        "parameters": {
          "type": "array",
          "items": {
            "type": "object"
          }
        },
        "variableType": {
          "type": "string",
          "description": "Variable type (e.g., Boolean, Float, Integer, Vector, String, Object)"
        },
        "defaultValue": {
          "type": "object",
          "description": "Generic value (any type).",
          "additionalProperties": true
        },
        "category": {
          "type": "string"
        },
        "isReplicated": {
          "type": "boolean"
        },
        "isPublic": {
          "type": "boolean"
        },
        "variablePinType": {
          "type": "object",
          "additionalProperties": true
        },
        "functionName": {
          "type": "string",
          "description": "Name of the function."
        },
        "inputs": {
          "type": "array",
          "items": {
            "type": "object"
          }
        },
        "outputs": {
          "type": "array",
          "items": {
            "type": "object"
          }
        },
        "posX": {
          "type": "number"
        },
        "posY": {
          "type": "number"
        },
        "eventName": {
          "type": "string",
          "description": "Name of the event."
        },
        "parentComponent": {
          "type": "string"
        },
        "meshPath": {
          "type": "string",
          "description": "Mesh asset path."
        },
        "materialPath": {
          "type": "string",
          "description": "Material asset path."
        },
        "transform": {
          "type": "object",
          "additionalProperties": true
        },
        "applyAndSave": {
          "type": "boolean"
        },
        "scriptName": {
          "type": "string"
        },
        "memberClass": {
          "type": "string"
        },
        "targetClass": {
          "type": "string"
        },
        "inputAxisName": {
          "type": "string"
        },
        "inputPin": {
          "type": "string",
          "description": "Name of the pin."
        },
        "outputPin": {
          "type": "string",
          "description": "Name of the pin."
        },
        "saveAfterCompile": {
          "type": "boolean"
        },
        "timeoutMs": {
          "type": "number"
        },
        "waitForCompletion": {
          "type": "boolean"
        },
        "waitForCompletionTimeoutMs": {
          "type": "number"
        },
        "parentClass": {
          "type": "string",
          "description": "Path or name of the parent class."
        },
        "fromNodeId": {
          "type": "string",
          "description": "ID of the source node."
        },
        "fromPin": {
          "type": "string",
          "description": "Name of the source pin."
        },
        "fromPinName": {
          "type": "string",
          "description": "Name of the source pin."
        },
        "toNodeId": {
          "type": "string",
          "description": "ID of the target node."
        },
        "toPin": {
          "type": "string",
          "description": "Name of the target pin."
        },
        "toPinName": {
          "type": "string",
          "description": "Name of the target pin."
        }
      },
      "required": [
        "action"
      ]
    }
  },
  {
    "name": "manage_character",
    "description": "Create Character Blueprints with movement, locomotion, and animation state machines.",
    "category": "gameplay",
    "inputSchema": {
      "type": "object",
      "properties": {
        "action": {
          "type": "string",
          "description": "Character action to perform.",
          "enum": [
            "create_character_blueprint",
            "configure_capsule_component",
            "configure_mesh_component",
            "configure_camera_component",
            "configure_movement_speeds",
            "configure_jump",
            "configure_rotation",
            "add_custom_movement_mode",
            "configure_nav_movement",
            "setup_mantling",
            "setup_vaulting",
            "setup_climbing",
            "setup_sliding",
            "setup_wall_running",
            "setup_grappling",
            "setup_footstep_system",
            "map_surface_to_sound",
            "configure_footstep_fx",
            "get_character_info",
            "setup_movement",
            "set_walk_speed",
            "set_jump_height",
            "set_gravity_scale",
            "set_ground_friction",
            "set_braking_deceleration",
            "configure_crouch",
            "configure_sprint"
          ]
        },
        "name": {
          "type": "string",
          "description": "Name of the asset to create."
        },
        "path": {
          "type": "string",
          "description": "Directory path for asset creation."
        },
        "blueprintPath": {
          "type": "string",
          "description": "Blueprint asset path."
        },
        "save": {
          "type": "boolean",
          "description": "Save the asset(s) after the operation."
        },
        "parentClass": {
          "type": "string",
          "description": "Parent class for character blueprint.",
          "enum": [
            "Character",
            "ACharacter",
            "PlayerCharacter",
            "AICharacter"
          ]
        },
        "skeletalMeshPath": {
          "type": "string",
          "description": "Skeletal mesh path."
        },
        "animBlueprintPath": {
          "type": "string",
          "description": "Path to animation blueprint."
        },
        "capsuleRadius": {
          "type": "number"
        },
        "capsuleHalfHeight": {
          "type": "number"
        },
        "meshOffset": {
          "type": "object",
          "description": "Mesh location offset.",
          "properties": {
            "x": {
              "type": "number"
            },
            "y": {
              "type": "number"
            },
            "z": {
              "type": "number"
            }
          }
        },
        "meshRotation": {
          "type": "object",
          "description": "Mesh rotation offset.",
          "properties": {
            "pitch": {
              "type": "number"
            },
            "yaw": {
              "type": "number"
            },
            "roll": {
              "type": "number"
            }
          }
        },
        "cameraSocketName": {
          "type": "string",
          "description": "Camera socket name."
        },
        "cameraOffset": {
          "type": "object",
          "description": "Camera location offset.",
          "properties": {
            "x": {
              "type": "number"
            },
            "y": {
              "type": "number"
            },
            "z": {
              "type": "number"
            }
          }
        },
        "cameraUsePawnControlRotation": {
          "type": "boolean",
          "description": "Camera follows controller rotation."
        },
        "springArmLength": {
          "type": "number"
        },
        "springArmLagEnabled": {
          "type": "boolean",
          "description": "Enable camera lag."
        },
        "springArmLagSpeed": {
          "type": "number",
          "description": "Camera lag speed."
        },
        "walkSpeed": {
          "type": "number"
        },
        "runSpeed": {
          "type": "number"
        },
        "sprintSpeed": {
          "type": "number"
        },
        "crouchSpeed": {
          "type": "number"
        },
        "swimSpeed": {
          "type": "number"
        },
        "flySpeed": {
          "type": "number"
        },
        "acceleration": {
          "type": "number"
        },
        "deceleration": {
          "type": "number"
        },
        "groundFriction": {
          "type": "number"
        },
        "jumpHeight": {
          "type": "number"
        },
        "airControl": {
          "type": "number"
        },
        "doubleJumpEnabled": {
          "type": "boolean",
          "description": "Enable double jump."
        },
        "maxJumpCount": {
          "type": "number"
        },
        "jumpHoldTime": {
          "type": "number",
          "description": "Max hold time for variable jump."
        },
        "gravityScale": {
          "type": "number"
        },
        "fallingLateralFriction": {
          "type": "number",
          "description": "Air friction."
        },
        "orientToMovement": {
          "type": "boolean",
          "description": "Orient rotation to movement direction."
        },
        "useControllerRotationYaw": {
          "type": "boolean",
          "description": "Use controller yaw rotation."
        },
        "useControllerRotationPitch": {
          "type": "boolean",
          "description": "Use controller pitch rotation."
        },
        "useControllerRotationRoll": {
          "type": "boolean",
          "description": "Use controller roll rotation."
        },
        "rotationRate": {
          "type": "number"
        },
        "modeName": {
          "type": "string",
          "description": "Name for custom movement mode."
        },
        "modeId": {
          "type": "number",
          "description": "Custom movement mode ID."
        },
        "navAgentRadius": {
          "type": "number"
        },
        "navAgentHeight": {
          "type": "number"
        },
        "avoidanceEnabled": {
          "type": "boolean",
          "description": "Enable AI avoidance."
        },
        "pathFollowingEnabled": {
          "type": "boolean",
          "description": "Enable path following."
        },
        "mantleHeight": {
          "type": "number",
          "description": "Maximum mantle height."
        },
        "mantleReachDistance": {
          "type": "number",
          "description": "Forward reach for mantle check."
        },
        "mantleAnimationPath": {
          "type": "string",
          "description": "Path to mantle animation montage."
        },
        "vaultHeight": {
          "type": "number",
          "description": "Maximum vault obstacle height."
        },
        "vaultDepth": {
          "type": "number",
          "description": "Obstacle depth to check."
        },
        "vaultAnimationPath": {
          "type": "string",
          "description": "Path to vault animation montage."
        },
        "climbSpeed": {
          "type": "number"
        },
        "climbableTag": {
          "type": "string",
          "description": "Tag for climbable surfaces."
        },
        "climbAnimationPath": {
          "type": "string",
          "description": "Path to climb animation."
        },
        "slideSpeed": {
          "type": "number"
        },
        "slideDuration": {
          "type": "number"
        },
        "slideCooldown": {
          "type": "number"
        },
        "slideAnimationPath": {
          "type": "string",
          "description": "Path to slide animation."
        },
        "wallRunSpeed": {
          "type": "number",
          "description": "Wall running speed."
        },
        "wallRunDuration": {
          "type": "number",
          "description": "Maximum wall run duration."
        },
        "wallRunGravityScale": {
          "type": "number",
          "description": "Gravity during wall run."
        },
        "wallRunAnimationPath": {
          "type": "string",
          "description": "Path to wall run animation."
        },
        "grappleRange": {
          "type": "number",
          "description": "Maximum grapple distance."
        },
        "grappleSpeed": {
          "type": "number",
          "description": "Grapple pull speed."
        },
        "grappleTargetTag": {
          "type": "string",
          "description": "Tag for grapple targets."
        },
        "grappleCablePath": {
          "type": "string",
          "description": "Path to cable mesh/material."
        },
        "footstepEnabled": {
          "type": "boolean",
          "description": "Enable footstep system."
        },
        "footstepSocketLeft": {
          "type": "string",
          "description": "Left foot socket name."
        },
        "footstepSocketRight": {
          "type": "string",
          "description": "Right foot socket name."
        },
        "footstepTraceDistance": {
          "type": "number",
          "description": "Ground trace distance."
        },
        "surfaceType": {
          "type": "string",
          "description": "Physical surface type.",
          "enum": [
            "Default",
            "Concrete",
            "Grass",
            "Dirt",
            "Metal",
            "Wood",
            "Water",
            "Snow",
            "Sand",
            "Gravel",
            "Custom"
          ]
        },
        "footstepSoundPath": {
          "type": "string",
          "description": "Path to footstep sound cue."
        },
        "footstepParticlePath": {
          "type": "string",
          "description": "Path to footstep particle."
        },
        "footstepDecalPath": {
          "type": "string",
          "description": "Path to footstep decal."
        }
      },
      "required": [
        "action"
      ]
    }
  },
  {
    "name": "manage_combat",
    "description": "Create weapons with hitscan/projectile firing, configure damage types, hitboxes, reload, and melee combat (combos, parry, block).",
    "category": "gameplay",
    "inputSchema": {
      "type": "object",
      "properties": {
        "action": {
          "type": "string",
          "description": "Combat action to perform",
          "enum": [
            "create_weapon_blueprint",
            "configure_weapon_mesh",
            "configure_weapon_sockets",
            "set_weapon_stats",
            "configure_hitscan",
            "configure_projectile",
            "configure_spread_pattern",
            "configure_recoil_pattern",
            "configure_aim_down_sights",
            "create_projectile_blueprint",
            "configure_projectile_movement",
            "configure_projectile_collision",
            "configure_projectile_homing",
            "create_damage_type",
            "configure_damage_execution",
            "setup_hitbox_component",
            "setup_reload_system",
            "setup_ammo_system",
            "setup_attachment_system",
            "setup_weapon_switching",
            "configure_muzzle_flash",
            "configure_tracer",
            "configure_impact_effects",
            "configure_shell_ejection",
            "create_melee_trace",
            "configure_combo_system",
            "create_hit_pause",
            "configure_hit_reaction",
            "setup_parry_block_system",
            "configure_weapon_trails",
            "get_combat_info",
            "setup_damage_type",
            "configure_hit_detection",
            "get_combat_stats",
            "create_damage_effect",
            "apply_damage",
            "heal",
            "create_shield",
            "modify_armor"
          ]
        },
        "blueprintPath": {
          "type": "string",
          "description": "Blueprint asset path."
        },
        "name": {
          "type": "string",
          "description": "Name identifier."
        },
        "path": {
          "type": "string",
          "description": "Directory path for asset creation."
        },
        "weaponMeshPath": {
          "type": "string",
          "description": "Path to weapon static/skeletal mesh."
        },
        "muzzleSocketName": {
          "type": "string",
          "description": "Muzzle socket name."
        },
        "ejectionSocketName": {
          "type": "string",
          "description": "Shell ejection socket name."
        },
        "attachmentSocketNames": {
          "type": "array",
          "description": "List of attachment socket names.",
          "items": {
            "type": "string"
          }
        },
        "baseDamage": {
          "type": "number"
        },
        "fireRate": {
          "type": "number"
        },
        "range": {
          "type": "number"
        },
        "spread": {
          "type": "number"
        },
        "hitscanEnabled": {
          "type": "boolean",
          "description": "Enable hitscan firing."
        },
        "traceChannel": {
          "type": "string",
          "description": "Trace channel for hitscan.",
          "enum": [
            "Visibility",
            "Camera",
            "Weapon",
            "Custom"
          ]
        },
        "projectileClass": {
          "type": "string",
          "description": "Projectile class path."
        },
        "spreadPattern": {
          "type": "string",
          "description": "Spread pattern type.",
          "enum": [
            "Random",
            "Fixed",
            "FixedWithRandom",
            "Shotgun"
          ]
        },
        "spreadIncrease": {
          "type": "number",
          "description": "Spread increase per shot."
        },
        "spreadRecovery": {
          "type": "number",
          "description": "Spread recovery rate."
        },
        "recoilPitch": {
          "type": "number",
          "description": "Vertical recoil (degrees)."
        },
        "recoilYaw": {
          "type": "number",
          "description": "Horizontal recoil (degrees)."
        },
        "recoilRecovery": {
          "type": "number",
          "description": "Recoil recovery speed."
        },
        "adsEnabled": {
          "type": "boolean",
          "description": "Enable aim down sights."
        },
        "adsFov": {
          "type": "number",
          "description": "FOV when aiming."
        },
        "adsSpeed": {
          "type": "number",
          "description": "Time to aim down sights."
        },
        "adsSpreadMultiplier": {
          "type": "number",
          "description": "Spread multiplier when aiming."
        },
        "projectileSpeed": {
          "type": "number"
        },
        "projectileGravityScale": {
          "type": "number"
        },
        "projectileLifespan": {
          "type": "number"
        },
        "projectileMeshPath": {
          "type": "string",
          "description": "Path to projectile mesh."
        },
        "collisionRadius": {
          "type": "number"
        },
        "bounceEnabled": {
          "type": "boolean",
          "description": "Enable projectile bouncing."
        },
        "bounceVelocityRatio": {
          "type": "number",
          "description": "Velocity retained on bounce (0-1)."
        },
        "homingEnabled": {
          "type": "boolean",
          "description": "Enable homing behavior."
        },
        "homingAcceleration": {
          "type": "number",
          "description": "Homing turn rate."
        },
        "homingTargetTag": {
          "type": "string",
          "description": "Tag for homing targets."
        },
        "damageTypeName": {
          "type": "string",
          "description": "Name for damage type."
        },
        "damageCategory": {
          "type": "string",
          "description": "Damage category.",
          "enum": [
            "Physical",
            "Fire",
            "Ice",
            "Electric",
            "Poison",
            "Explosion",
            "Radial",
            "Custom"
          ]
        },
        "damageImpulse": {
          "type": "number",
          "description": "Impulse applied on hit."
        },
        "criticalMultiplier": {
          "type": "number",
          "description": "Critical hit damage multiplier."
        },
        "headshotMultiplier": {
          "type": "number",
          "description": "Headshot damage multiplier."
        },
        "hitboxBoneName": {
          "type": "string",
          "description": "Bone name for hitbox."
        },
        "hitboxType": {
          "type": "string",
          "description": "Hitbox collision shape.",
          "enum": [
            "Capsule",
            "Box",
            "Sphere"
          ]
        },
        "hitboxSize": {
          "type": "object",
          "description": "Hitbox dimensions.",
          "properties": {
            "radius": {
              "type": "number"
            },
            "halfHeight": {
              "type": "number"
            },
            "extent": {
              "type": "object",
              "description": "3D extent (half-size).",
              "properties": {
                "x": {
                  "type": "number"
                },
                "y": {
                  "type": "number"
                },
                "z": {
                  "type": "number"
                }
              }
            }
          }
        },
        "isDamageZoneHead": {
          "type": "boolean",
          "description": "Mark as headshot zone."
        },
        "damageMultiplier": {
          "type": "number",
          "description": "Damage multiplier for this hitbox."
        },
        "magazineSize": {
          "type": "number"
        },
        "reloadTime": {
          "type": "number"
        },
        "reloadAnimationPath": {
          "type": "string",
          "description": "Path to reload animation."
        },
        "ammoType": {
          "type": "string",
          "description": "Ammo type identifier."
        },
        "maxAmmo": {
          "type": "number"
        },
        "startingAmmo": {
          "type": "number"
        },
        "attachmentSlots": {
          "type": "array",
          "description": "Attachment slot definitions.",
          "items": {
            "type": "object"
          }
        },
        "switchInTime": {
          "type": "number",
          "description": "Time to equip weapon."
        },
        "switchOutTime": {
          "type": "number",
          "description": "Time to unequip weapon."
        },
        "switchInAnimationPath": {
          "type": "string",
          "description": "Path to equip animation."
        },
        "switchOutAnimationPath": {
          "type": "string",
          "description": "Path to unequip animation."
        },
        "muzzleFlashParticlePath": {
          "type": "string",
          "description": "Path to muzzle flash particle."
        },
        "muzzleFlashScale": {
          "type": "number",
          "description": "Muzzle flash scale."
        },
        "muzzleSoundPath": {
          "type": "string",
          "description": "Path to firing sound."
        },
        "tracerParticlePath": {
          "type": "string",
          "description": "Path to tracer particle."
        },
        "tracerSpeed": {
          "type": "number",
          "description": "Tracer travel speed."
        },
        "impactParticlePath": {
          "type": "string",
          "description": "Path to impact particle."
        },
        "impactSoundPath": {
          "type": "string",
          "description": "Path to impact sound."
        },
        "impactDecalPath": {
          "type": "string",
          "description": "Path to impact decal."
        },
        "shellMeshPath": {
          "type": "string",
          "description": "Path to shell casing mesh."
        },
        "shellEjectionForce": {
          "type": "number",
          "description": "Shell ejection impulse."
        },
        "shellLifespan": {
          "type": "number",
          "description": "Shell casing lifetime."
        },
        "meleeTraceStartSocket": {
          "type": "string",
          "description": "Socket for trace start."
        },
        "meleeTraceEndSocket": {
          "type": "string",
          "description": "Socket for trace end."
        },
        "meleeTraceRadius": {
          "type": "number",
          "description": "Sphere trace radius."
        },
        "meleeTraceChannel": {
          "type": "string",
          "description": "Trace channel for melee."
        },
        "comboWindowTime": {
          "type": "number",
          "description": "Time window for combo input."
        },
        "maxComboCount": {
          "type": "number",
          "description": "Maximum combo length."
        },
        "comboAnimations": {
          "type": "array",
          "description": "Paths to combo attack animations.",
          "items": {
            "type": "string"
          }
        },
        "hitPauseDuration": {
          "type": "number",
          "description": "Hitstop duration in seconds."
        },
        "hitPauseTimeDilation": {
          "type": "number",
          "description": "Time dilation during hitstop."
        },
        "hitReactionMontage": {
          "type": "string",
          "description": "Path to hit reaction montage."
        },
        "hitReactionStunTime": {
          "type": "number",
          "description": "Stun duration on hit."
        },
        "parryWindowStart": {
          "type": "number",
          "description": "Parry window start time (normalized)."
        },
        "parryWindowEnd": {
          "type": "number",
          "description": "Parry window end time (normalized)."
        },
        "parryAnimationPath": {
          "type": "string",
          "description": "Path to parry animation."
        },
        "blockDamageReduction": {
          "type": "number",
          "description": "Damage reduction when blocking (0-1)."
        },
        "blockStaminaCost": {
          "type": "number",
          "description": "Stamina cost per blocked hit."
        },
        "weaponTrailParticlePath": {
          "type": "string",
          "description": "Path to weapon trail particle."
        },
        "weaponTrailStartSocket": {
          "type": "string",
          "description": "Trail start socket."
        },
        "weaponTrailEndSocket": {
          "type": "string",
          "description": "Trail end socket."
        }
      },
      "required": [
        "action"
      ]
    }
  },
  {
    "name": "manage_effect",
    "description": "Niagara particle systems, VFX, debug shapes, and GPU simulations. Create systems, emitters, modules, and control particle effects.",
    "category": "gameplay",
    "inputSchema": {
      "type": "object",
      "properties": {
        "action": {
          "type": "string",
          "description": "Effect/Niagara action to perform.",
          "enum": [
            "particle",
            "niagara",
            "debug_shape",
            "spawn_niagara",
            "create_dynamic_light",
            "create_niagara_system",
            "create_niagara_emitter",
            "create_volumetric_fog",
            "create_particle_trail",
            "create_environment_effect",
            "create_impact_effect",
            "create_niagara_ribbon",
            "activate",
            "activate_effect",
            "deactivate",
            "reset",
            "advance_simulation",
            "add_niagara_module",
            "connect_niagara_pins",
            "remove_niagara_node",
            "set_niagara_parameter",
            "clear_debug_shapes",
            "cleanup",
            "list_debug_shapes",
            "add_emitter_to_system",
            "set_emitter_properties",
            "add_spawn_rate_module",
            "add_spawn_burst_module",
            "add_spawn_per_unit_module",
            "add_initialize_particle_module",
            "add_particle_state_module",
            "add_force_module",
            "add_velocity_module",
            "add_acceleration_module",
            "add_size_module",
            "add_color_module",
            "add_sprite_renderer_module",
            "add_mesh_renderer_module",
            "add_ribbon_renderer_module",
            "add_light_renderer_module",
            "add_collision_module",
            "add_kill_particles_module",
            "add_camera_offset_module",
            "add_user_parameter",
            "set_parameter_value",
            "bind_parameter_to_source",
            "add_skeletal_mesh_data_interface",
            "add_static_mesh_data_interface",
            "add_spline_data_interface",
            "add_audio_spectrum_data_interface",
            "add_collision_query_data_interface",
            "add_event_generator",
            "add_event_receiver",
            "configure_event_payload",
            "enable_gpu_simulation",
            "add_simulation_stage",
            "get_niagara_info",
            "validate_niagara_system"
          ]
        },
        "name": {
          "type": "string",
          "description": "Name identifier."
        },
        "assetPath": {
          "type": "string",
          "description": "Asset path (e.g., /Game/Path/Asset)."
        },
        "savePath": {
          "type": "string",
          "description": "Path to save the asset."
        },
        "template": {
          "type": "string"
        },
        "system": {
          "type": "string",
          "description": "Asset path (e.g., /Game/Path/Asset)."
        },
        "systemPath": {
          "type": "string",
          "description": "Asset path (e.g., /Game/Path/Asset)."
        },
        "systemName": {
          "type": "string"
        },
        "emitter": {
          "type": "string"
        },
        "emitterName": {
          "type": "string"
        },
        "emitterTemplate": {
          "type": "string",
          "description": "Asset path (e.g., /Game/Path/Asset)."
        },
        "location": {
          "type": "object",
          "description": "3D location (x, y, z).",
          "properties": {
            "x": {
              "type": "number"
            },
            "y": {
              "type": "number"
            },
            "z": {
              "type": "number"
            }
          }
        },
        "rotation": {
          "type": "object",
          "description": "3D rotation (pitch, yaw, roll).",
          "properties": {
            "pitch": {
              "type": "number"
            },
            "yaw": {
              "type": "number"
            },
            "roll": {
              "type": "number"
            }
          }
        },
        "scale": {
          "type": "object",
          "description": "3D scale (x, y, z).",
          "properties": {
            "x": {
              "type": "number"
            },
            "y": {
              "type": "number"
            },
            "z": {
              "type": "number"
            }
          }
        },
        "effect": {
          "type": "string",
          "description": "Asset path (e.g., /Game/Path/Asset)."
        },
        "effectId": {
          "type": "string"
        },
        "effectHandle": {
          "type": "string"
        },
        "niagaraHandle": {
          "type": "string"
        },
        "actorName": {
          "type": "string",
          "description": "Name of the actor."
        },
        "reset": {
          "type": "boolean"
        },
        "time": {
          "type": "number"
        },
        "shape": {
          "type": "string"
        },
        "shapeType": {
          "type": "string"
        },
        "radius": {
          "type": "number"
        },
        "color": {
          "type": "array",
          "items": {
            "type": "number"
          }
        },
        "duration": {
          "type": "number"
        },
        "lightType": {
          "type": "string"
        },
        "intensity": {
          "type": "number"
        },
        "preset": {
          "type": "string"
        },
        "type": {
          "type": "string"
        },
        "width": {
          "type": "number"
        },
        "density": {
          "type": "number"
        },
        "scattering": {
          "type": "number"
        },
        "attachTo": {
          "type": "string"
        },
        "ribbonPath": {
          "type": "string",
          "description": "Asset path (e.g., /Game/Path/Asset)."
        },
        "surfaceType": {
          "type": "string"
        },
        "impactType": {
          "type": "string"
        },
        "effectType": {
          "type": "string"
        },
        "parameterName": {
          "type": "string",
          "description": "Name of the parameter."
        },
        "parameterType": {
          "type": "string"
        },
        "value": {
          "type": "object",
          "description": "Generic value (any type).",
          "additionalProperties": true
        },
        "moduleName": {
          "type": "string"
        },
        "fromNode": {
          "type": "string"
        },
        "toNode": {
          "type": "string"
        },
        "fromPin": {
          "type": "string",
          "description": "Name of the source pin."
        },
        "toPin": {
          "type": "string",
          "description": "Name of the target pin."
        },
        "outputPin": {
          "type": "string",
          "description": "Name of the source pin."
        },
        "inputPin": {
          "type": "string",
          "description": "Name of the target pin."
        },
        "node": {
          "type": "string"
        },
        "loopBehavior": {
          "type": "string"
        },
        "spawnRate": {
          "type": "number"
        },
        "count": {
          "type": "number"
        },
        "loopCount": {
          "type": "number"
        },
        "unitsPerSpawn": {
          "type": "number"
        },
        "attributes": {
          "type": "object",
          "additionalProperties": true
        },
        "updateScript": {
          "type": "string"
        },
        "forceType": {
          "type": "string"
        },
        "strength": {
          "type": "number"
        },
        "velocityMode": {
          "type": "string"
        },
        "speedMin": {
          "type": "number"
        },
        "speedMax": {
          "type": "number"
        },
        "acceleration": {
          "type": "object",
          "description": "3D acceleration vector (x, y, z).",
          "properties": {
            "x": {
              "type": "number"
            },
            "y": {
              "type": "number"
            },
            "z": {
              "type": "number"
            }
          }
        },
        "sizeMode": {
          "type": "string"
        },
        "sizeMin": {
          "type": "object",
          "description": "Minimum particle size (x, y, z).",
          "properties": {
            "x": {
              "type": "number"
            },
            "y": {
              "type": "number"
            },
            "z": {
              "type": "number"
            }
          }
        },
        "sizeMax": {
          "type": "object",
          "description": "Maximum particle size (x, y, z).",
          "properties": {
            "x": {
              "type": "number"
            },
            "y": {
              "type": "number"
            },
            "z": {
              "type": "number"
            }
          }
        },
        "colorMode": {
          "type": "string"
        },
        "gradientStart": {
          "type": "array",
          "items": {
            "type": "number"
          }
        },
        "gradientEnd": {
          "type": "array",
          "items": {
            "type": "number"
          }
        },
        "material": {
          "type": "string",
          "description": "Material asset path."
        },
        "mesh": {
          "type": "string",
          "description": "Mesh asset path."
        },
        "lightIntensity": {
          "type": "number"
        },
        "lightRadius": {
          "type": "number"
        },
        "collisionMode": {
          "type": "string"
        },
        "collisionRadius": {
          "type": "number"
        },
        "killCondition": {
          "type": "string"
        },
        "offsetMode": {
          "type": "string"
        },
        "offsetAmount": {
          "type": "number"
        },
        "paramName": {
          "type": "string"
        },
        "paramType": {
          "type": "string"
        },
        "sourceActor": {
          "type": "string"
        },
        "skeletalMesh": {
          "type": "string",
          "description": "Mesh asset path."
        },
        "staticMesh": {
          "type": "string",
          "description": "Mesh asset path."
        },
        "splineComponent": {
          "type": "string"
        },
        "audioComponent": {
          "type": "string"
        },
        "queryChannel": {
          "type": "string"
        },
        "eventName": {
          "type": "string",
          "description": "Name of the event."
        },
        "condition": {
          "type": "string"
        },
        "receiverScript": {
          "type": "string"
        },
        "payload": {
          "type": "object",
          "additionalProperties": true
        },
        "enabled": {
          "type": "boolean"
        },
        "stageName": {
          "type": "string"
        },
        "stageType": {
          "type": "string"
        },
        "timeoutMs": {
          "type": "number"
        }
      },
      "required": [
        "action"
      ]
    }
  },
  {
    "name": "manage_game_framework",
    "description": "Create GameMode, GameState, PlayerController, PlayerState Blueprints. Configure match flow, teams, scoring, and spawning.",
    "category": "utility",
    "inputSchema": {
      "type": "object",
      "properties": {
        "action": {
          "type": "string",
          "description": "Game framework action to perform.",
          "enum": [
            "create_game_mode",
            "create_game_state",
            "create_player_controller",
            "create_player_state",
            "create_game_instance",
            "create_hud_class",
            "set_default_pawn_class",
            "set_player_controller_class",
            "set_game_state_class",
            "set_player_state_class",
            "configure_game_rules",
            "setup_match_states",
            "configure_round_system",
            "configure_team_system",
            "configure_scoring_system",
            "configure_spawn_system",
            "configure_player_start",
            "set_respawn_rules",
            "configure_spectating",
            "get_game_framework_info"
          ]
        },
        "name": {
          "type": "string",
          "description": "Name identifier."
        },
        "path": {
          "type": "string",
          "description": "Directory path for asset creation."
        },
        "gameModeBlueprint": {
          "type": "string",
          "description": "Path to GameMode blueprint to configure."
        },
        "blueprintPath": {
          "type": "string",
          "description": "Blueprint asset path."
        },
        "levelPath": {
          "type": "string",
          "description": "Level asset path."
        },
        "parentClass": {
          "type": "string",
          "description": "Path or name of the parent class."
        },
        "pawnClass": {
          "type": "string",
          "description": "Pawn class to use."
        },
        "defaultPawnClass": {
          "type": "string",
          "description": "Default pawn class for GameMode."
        },
        "playerControllerClass": {
          "type": "string",
          "description": "PlayerController class path."
        },
        "gameStateClass": {
          "type": "string",
          "description": "GameState class path."
        },
        "playerStateClass": {
          "type": "string",
          "description": "PlayerState class path."
        },
        "spectatorClass": {
          "type": "string",
          "description": "Spectator pawn class."
        },
        "hudClass": {
          "type": "string",
          "description": "HUD class path."
        },
        "timeLimit": {
          "type": "number"
        },
        "scoreLimit": {
          "type": "number"
        },
        "bDelayedStart": {
          "type": "boolean",
          "description": "Whether to delay match start."
        },
        "startPlayersNeeded": {
          "type": "number"
        },
        "states": {
          "type": "array",
          "description": "Match state definitions.",
          "items": {
            "type": "object"
          }
        },
        "numRounds": {
          "type": "number"
        },
        "roundTime": {
          "type": "number"
        },
        "intermissionTime": {
          "type": "number"
        },
        "numTeams": {
          "type": "number"
        },
        "teamSize": {
          "type": "number"
        },
        "autoBalance": {
          "type": "boolean",
          "description": "Enable automatic team balancing."
        },
        "friendlyFire": {
          "type": "boolean",
          "description": "Enable friendly fire damage."
        },
        "teamIndex": {
          "type": "number",
          "description": "Team index for PlayerStart."
        },
        "scorePerKill": {
          "type": "number",
          "description": "Points awarded per kill."
        },
        "scorePerObjective": {
          "type": "number",
          "description": "Points awarded per objective."
        },
        "scorePerAssist": {
          "type": "number",
          "description": "Points awarded per assist."
        },
        "spawnSelectionMethod": {
          "type": "string",
          "description": "How to select spawn points.",
          "enum": [
            "Random",
            "RoundRobin",
            "FarthestFromEnemies"
          ]
        },
        "respawnDelay": {
          "type": "number"
        },
        "respawnLocation": {
          "type": "string",
          "description": "Where players respawn.",
          "enum": [
            "PlayerStart",
            "LastDeath",
            "TeamBase"
          ]
        },
        "respawnConditions": {
          "type": "array",
          "description": "Conditions for respawn (e.g., \"RoundEnd\", \"Manual\").",
          "items": {
            "type": "string"
          }
        },
        "usePlayerStarts": {
          "type": "boolean",
          "description": "Use PlayerStart actors."
        },
        "location": {
          "type": "object",
          "description": "Spawn location.",
          "properties": {
            "x": {
              "type": "number"
            },
            "y": {
              "type": "number"
            },
            "z": {
              "type": "number"
            }
          }
        },
        "rotation": {
          "type": "object",
          "description": "Spawn rotation.",
          "properties": {
            "pitch": {
              "type": "number"
            },
            "yaw": {
              "type": "number"
            },
            "roll": {
              "type": "number"
            }
          }
        },
        "bPlayerOnly": {
          "type": "boolean",
          "description": "Restrict to players only."
        },
        "allowSpectating": {
          "type": "boolean",
          "description": "Allow spectator mode."
        },
        "spectatorViewMode": {
          "type": "string",
          "description": "Spectator view mode.",
          "enum": [
            "FreeCam",
            "ThirdPerson",
            "FirstPerson",
            "DeathCam"
          ]
        },
        "save": {
          "type": "boolean",
          "description": "Save the asset(s) after the operation."
        }
      },
      "required": [
        "action"
      ]
    }
  },
  {
    "name": "manage_gas",
    "description": "Create Gameplay Abilities, Effects, Attribute Sets, and Gameplay Cues for ability systems.",
    "category": "gameplay",
    "inputSchema": {
      "type": "object",
      "properties": {
        "action": {
          "type": "string",
          "description": "GAS action to perform.",
          "enum": [
            "add_ability_system_component",
            "configure_asc",
            "create_attribute_set",
            "add_attribute",
            "set_attribute_base_value",
            "set_attribute_clamping",
            "create_gameplay_ability",
            "set_ability_tags",
            "set_ability_costs",
            "set_ability_cooldown",
            "set_ability_targeting",
            "add_ability_task",
            "set_activation_policy",
            "set_instancing_policy",
            "create_gameplay_effect",
            "set_effect_duration",
            "add_effect_modifier",
            "set_modifier_magnitude",
            "add_effect_execution_calculation",
            "add_effect_cue",
            "set_effect_stacking",
            "set_effect_tags",
            "create_gameplay_cue_notify",
            "configure_cue_trigger",
            "set_cue_effects",
            "add_tag_to_asset",
            "get_gas_info"
          ]
        },
        "name": {
          "type": "string",
          "description": "Name of the asset to create."
        },
        "path": {
          "type": "string",
          "description": "Directory path for asset creation."
        },
        "assetPath": {
          "type": "string",
          "description": "Asset path (e.g., /Game/Path/Asset)."
        },
        "blueprintPath": {
          "type": "string",
          "description": "Blueprint asset path."
        },
        "save": {
          "type": "boolean",
          "description": "Save the asset(s) after the operation."
        },
        "replicationMode": {
          "type": "string",
          "description": "ASC replication mode.",
          "enum": [
            "Full",
            "Minimal",
            "Mixed"
          ]
        },
        "ownerActor": {
          "type": "string",
          "description": "Owner actor class for ASC."
        },
        "avatarActor": {
          "type": "string",
          "description": "Avatar actor class for ASC."
        },
        "attributeSetPath": {
          "type": "string",
          "description": "Path to Attribute Set asset."
        },
        "attributeName": {
          "type": "string",
          "description": "Name of the attribute."
        },
        "attributeType": {
          "type": "string",
          "description": "Predefined attribute type or Custom.",
          "enum": [
            "Health",
            "MaxHealth",
            "Mana",
            "MaxMana",
            "Stamina",
            "MaxStamina",
            "Damage",
            "Armor",
            "AttackPower",
            "MoveSpeed",
            "Custom"
          ]
        },
        "baseValue": {
          "type": "number",
          "description": "Base value for attribute."
        },
        "minValue": {
          "type": "number",
          "description": "Minimum value for clamping."
        },
        "maxValue": {
          "type": "number",
          "description": "Maximum value for clamping."
        },
        "clampMode": {
          "type": "string",
          "description": "Attribute clamping mode.",
          "enum": [
            "None",
            "Min",
            "Max",
            "MinMax"
          ]
        },
        "abilityPath": {
          "type": "string",
          "description": "Path to ability asset."
        },
        "parentClass": {
          "type": "string",
          "description": "Path or name of the parent class."
        },
        "abilityTags": {
          "type": "array",
          "description": "Gameplay tags for this ability.",
          "items": {
            "type": "string"
          }
        },
        "cancelAbilitiesWithTag": {
          "type": "array",
          "description": "Tags of abilities to cancel when this activates.",
          "items": {
            "type": "string"
          }
        },
        "blockAbilitiesWithTag": {
          "type": "array",
          "description": "Tags of abilities blocked while this is active.",
          "items": {
            "type": "string"
          }
        },
        "activationRequiredTags": {
          "type": "array",
          "description": "Tags required to activate this ability.",
          "items": {
            "type": "string"
          }
        },
        "activationBlockedTags": {
          "type": "array",
          "description": "Tags that block activation of this ability.",
          "items": {
            "type": "string"
          }
        },
        "costEffectPath": {
          "type": "string",
          "description": "Path to cost Gameplay Effect."
        },
        "costAttribute": {
          "type": "string",
          "description": "Attribute used for cost (e.g., Mana)."
        },
        "costMagnitude": {
          "type": "number",
          "description": "Cost magnitude."
        },
        "cooldownEffectPath": {
          "type": "string",
          "description": "Path to cooldown Gameplay Effect."
        },
        "cooldownDuration": {
          "type": "number",
          "description": "Cooldown duration in seconds."
        },
        "cooldownTags": {
          "type": "array",
          "description": "Tags applied during cooldown.",
          "items": {
            "type": "string"
          }
        },
        "targetingMode": {
          "type": "string",
          "description": "Targeting mode for ability.",
          "enum": [
            "None",
            "SingleTarget",
            "AOE",
            "Directional",
            "Ground",
            "ActorPlacement"
          ]
        },
        "targetRange": {
          "type": "number",
          "description": "Maximum targeting range."
        },
        "aoeRadius": {
          "type": "number",
          "description": "Area of effect radius."
        },
        "taskType": {
          "type": "string",
          "description": "Type of ability task to add.",
          "enum": [
            "WaitDelay",
            "WaitInputPress",
            "WaitInputRelease",
            "WaitGameplayEvent",
            "WaitTargetData",
            "WaitConfirmCancel",
            "PlayMontageAndWait",
            "ApplyRootMotionConstantForce",
            "WaitMovementModeChange"
          ]
        },
        "taskSettings": {
          "type": "object",
          "description": "Task-specific settings.",
          "additionalProperties": true
        },
        "activationPolicy": {
          "type": "string",
          "description": "When the ability activates.",
          "enum": [
            "OnInputPressed",
            "WhileInputActive",
            "OnSpawn",
            "OnGiven"
          ]
        },
        "instancingPolicy": {
          "type": "string",
          "description": "How the ability is instanced.",
          "enum": [
            "NonInstanced",
            "InstancedPerActor",
            "InstancedPerExecution"
          ]
        },
        "effectPath": {
          "type": "string",
          "description": "Path to effect asset."
        },
        "durationType": {
          "type": "string",
          "description": "Effect duration type.",
          "enum": [
            "Instant",
            "Infinite",
            "HasDuration"
          ]
        },
        "duration": {
          "type": "number",
          "description": "Duration in seconds."
        },
        "period": {
          "type": "number",
          "description": "Period for periodic effects."
        },
        "modifierOperation": {
          "type": "string",
          "description": "Modifier operation on attribute.",
          "enum": [
            "Add",
            "Multiply",
            "Divide",
            "Override"
          ]
        },
        "modifierMagnitude": {
          "type": "number",
          "description": "Magnitude of the modifier."
        },
        "magnitudeCalculationType": {
          "type": "string",
          "description": "How magnitude is calculated.",
          "enum": [
            "ScalableFloat",
            "AttributeBased",
            "SetByCaller",
            "CustomCalculationClass"
          ]
        },
        "setByCallerTag": {
          "type": "string",
          "description": "Tag for SetByCaller magnitude."
        },
        "coefficient": {
          "type": "number",
          "description": "Coefficient for attribute-based calculation."
        },
        "preMultiplyAdditiveValue": {
          "type": "number",
          "description": "Value added before multiplication."
        },
        "postMultiplyAdditiveValue": {
          "type": "number",
          "description": "Value added after multiplication."
        },
        "sourceAttribute": {
          "type": "string",
          "description": "Source attribute for attribute-based calculation."
        },
        "targetAttribute": {
          "type": "string",
          "description": "Target attribute for modifier."
        },
        "calculationClass": {
          "type": "string",
          "description": "UGameplayEffectExecutionCalculation class path."
        },
        "cueTag": {
          "type": "string",
          "description": "Gameplay Cue tag (e.g., GameplayCue.Damage.Fire)."
        },
        "cuePath": {
          "type": "string",
          "description": "Path to Gameplay Cue asset."
        },
        "stackingType": {
          "type": "string",
          "description": "Stacking type for effect.",
          "enum": [
            "None",
            "AggregateBySource",
            "AggregateByTarget"
          ]
        },
        "stackLimitCount": {
          "type": "number",
          "description": "Maximum stack count."
        },
        "stackDurationRefreshPolicy": {
          "type": "string",
          "description": "When to refresh stack duration.",
          "enum": [
            "RefreshOnSuccessfulApplication",
            "NeverRefresh"
          ]
        },
        "stackPeriodResetPolicy": {
          "type": "string",
          "description": "When to reset stack period.",
          "enum": [
            "ResetOnSuccessfulApplication",
            "NeverReset"
          ]
        },
        "stackExpirationPolicy": {
          "type": "string",
          "description": "What happens when stack expires.",
          "enum": [
            "ClearEntireStack",
            "RemoveSingleStackAndRefreshDuration",
            "RefreshDuration"
          ]
        },
        "grantedTags": {
          "type": "array",
          "description": "Tags granted while effect is active.",
          "items": {
            "type": "string"
          }
        },
        "applicationRequiredTags": {
          "type": "array",
          "description": "Tags required to apply this effect.",
          "items": {
            "type": "string"
          }
        },
        "removalTags": {
          "type": "array",
          "description": "Tags that cause effect removal.",
          "items": {
            "type": "string"
          }
        },
        "immunityTags": {
          "type": "array",
          "description": "Tags that block this effect.",
          "items": {
            "type": "string"
          }
        },
        "cueType": {
          "type": "string",
          "description": "Type of gameplay cue notify.",
          "enum": [
            "Static",
            "Actor"
          ]
        },
        "triggerType": {
          "type": "string",
          "description": "When the cue triggers.",
          "enum": [
            "OnActive",
            "WhileActive",
            "Executed",
            "OnRemove"
          ]
        },
        "particleSystemPath": {
          "type": "string",
          "description": "Path to particle system."
        },
        "soundPath": {
          "type": "string",
          "description": "Sound asset path."
        },
        "cameraShakePath": {
          "type": "string",
          "description": "Path to camera shake asset."
        },
        "decalPath": {
          "type": "string",
          "description": "Path to decal material."
        },
        "tagName": {
          "type": "string",
          "description": "Name of the tag."
        }
      },
      "required": [
        "action"
      ]
    }
  },
  {
    "name": "manage_geometry",
    "description": "Create procedural meshes using Geometry Script: booleans, deformers, UVs, collision, and LOD generation.",
    "category": "world",
    "inputSchema": {
      "type": "object",
      "properties": {
        "action": {
          "type": "string",
          "description": "Geometry action to perform",
          "enum": [
            "create_box",
            "create_sphere",
            "create_cylinder",
            "create_cone",
            "create_capsule",
            "create_torus",
            "create_plane",
            "create_disc",
            "create_stairs",
            "create_spiral_stairs",
            "create_ring",
            "create_arch",
            "create_pipe",
            "create_ramp",
            "boolean_union",
            "boolean_subtract",
            "boolean_intersection",
            "boolean_trim",
            "self_union",
            "extrude",
            "inset",
            "outset",
            "bevel",
            "offset_faces",
            "shell",
            "revolve",
            "chamfer",
            "extrude_along_spline",
            "bridge",
            "loft",
            "sweep",
            "duplicate_along_spline",
            "loop_cut",
            "edge_split",
            "quadrangulate",
            "bend",
            "twist",
            "taper",
            "noise_deform",
            "smooth",
            "relax",
            "stretch",
            "spherify",
            "cylindrify",
            "triangulate",
            "poke",
            "mirror",
            "array_linear",
            "array_radial",
            "simplify_mesh",
            "subdivide",
            "remesh_uniform",
            "merge_vertices",
            "remesh_voxel",
            "weld_vertices",
            "fill_holes",
            "remove_degenerates",
            "auto_uv",
            "project_uv",
            "transform_uvs",
            "unwrap_uv",
            "pack_uv_islands",
            "recalculate_normals",
            "flip_normals",
            "recompute_tangents",
            "generate_collision",
            "generate_complex_collision",
            "simplify_collision",
            "generate_lods",
            "set_lod_settings",
            "set_lod_screen_sizes",
            "convert_to_nanite",
            "convert_to_static_mesh",
            "get_mesh_info"
          ]
        },
        "meshPath": {
          "type": "string",
          "description": "Mesh asset path."
        },
        "targetMeshPath": {
          "type": "string",
          "description": "Path to second mesh for boolean operations."
        },
        "outputPath": {
          "type": "string",
          "description": "Output file or directory path."
        },
        "actorName": {
          "type": "string",
          "description": "Name of the actor."
        },
        "width": {
          "type": "number",
          "description": "Width value."
        },
        "height": {
          "type": "number",
          "description": "Height value."
        },
        "depth": {
          "type": "number",
          "description": "Depth value."
        },
        "radius": {
          "type": "number",
          "description": "Radius value."
        },
        "innerRadius": {
          "type": "number",
          "description": "Inner radius for torus."
        },
        "numSides": {
          "type": "number",
          "description": "Number of sides for cylinder, cone, etc."
        },
        "numRings": {
          "type": "number",
          "description": "Number of rings for sphere, torus."
        },
        "numSteps": {
          "type": "number",
          "description": "Number of steps for stairs."
        },
        "stepWidth": {
          "type": "number",
          "description": "Width of each stair step."
        },
        "stepHeight": {
          "type": "number",
          "description": "Height of each stair step."
        },
        "stepDepth": {
          "type": "number",
          "description": "Depth of each stair step."
        },
        "numTurns": {
          "type": "number",
          "description": "Number of turns for spiral."
        },
        "widthSegments": {
          "type": "number",
          "description": "Segments along width."
        },
        "heightSegments": {
          "type": "number",
          "description": "Segments along height."
        },
        "depthSegments": {
          "type": "number",
          "description": "Segments along depth."
        },
        "radialSegments": {
          "type": "number",
          "description": "Radial segments for circular shapes."
        },
        "location": {
          "type": "object",
          "description": "3D location (x, y, z).",
          "properties": {
            "x": {
              "type": "number"
            },
            "y": {
              "type": "number"
            },
            "z": {
              "type": "number"
            }
          }
        },
        "rotation": {
          "type": "object",
          "description": "3D rotation (pitch, yaw, roll).",
          "properties": {
            "pitch": {
              "type": "number"
            },
            "yaw": {
              "type": "number"
            },
            "roll": {
              "type": "number"
            }
          }
        },
        "scale": {
          "type": "object",
          "description": "3D scale (x, y, z).",
          "properties": {
            "x": {
              "type": "number"
            },
            "y": {
              "type": "number"
            },
            "z": {
              "type": "number"
            }
          }
        },
        "distance": {
          "type": "number",
          "description": "Distance value."
        },
        "amount": {
          "type": "number",
          "description": "Generic amount for operations (bevel size, inset distance, etc.)."
        },
        "segments": {
          "type": "number",
          "description": "Number of segments for bevel, subdivide."
        },
        "angle": {
          "type": "number",
          "description": "Angle in degrees."
        },
        "axis": {
          "type": "string",
          "description": "Axis for deformation operations.",
          "enum": [
            "X",
            "Y",
            "Z"
          ]
        },
        "strength": {
          "type": "number",
          "description": "Strength or weight."
        },
        "iterations": {
          "type": "number",
          "description": "Number of iterations for smooth, remesh."
        },
        "targetTriangleCount": {
          "type": "number",
          "description": "Target triangle count for simplification."
        },
        "targetEdgeLength": {
          "type": "number",
          "description": "Target edge length for remeshing."
        },
        "weldDistance": {
          "type": "number",
          "description": "Distance threshold for vertex welding."
        },
        "faceIndices": {
          "type": "array",
          "description": "Array of face indices.",
          "items": {
            "type": "number"
          }
        },
        "edgeIndices": {
          "type": "array",
          "description": "Array of edge indices.",
          "items": {
            "type": "number"
          }
        },
        "vertexIndices": {
          "type": "array",
          "description": "Array of vertex indices.",
          "items": {
            "type": "number"
          }
        },
        "selectionBox": {
          "type": "object",
          "description": "Bounding box for selection.",
          "properties": {
            "min": {
              "type": "object",
              "additionalProperties": true
            },
            "max": {
              "type": "object",
              "additionalProperties": true
            }
          }
        },
        "uvChannel": {
          "type": "number",
          "description": "UV channel index (0-7)."
        },
        "uvScale": {
          "type": "object",
          "description": "UV scale.",
          "properties": {
            "u": {
              "type": "number"
            },
            "v": {
              "type": "number"
            }
          }
        },
        "uvOffset": {
          "type": "object",
          "description": "UV offset.",
          "properties": {
            "u": {
              "type": "number"
            },
            "v": {
              "type": "number"
            }
          }
        },
        "projectionDirection": {
          "type": "string",
          "description": "Projection direction for UV.",
          "enum": [
            "X",
            "Y",
            "Z",
            "Auto"
          ]
        },
        "hardEdgeAngle": {
          "type": "number",
          "description": "Angle threshold for hard edges (degrees)."
        },
        "computeWeightedNormals": {
          "type": "boolean",
          "description": "Use area-weighted normals."
        },
        "smoothingGroupId": {
          "type": "number",
          "description": "Smoothing group ID."
        },
        "collisionType": {
          "type": "string",
          "description": "Collision complexity type.",
          "enum": [
            "Default",
            "Simple",
            "Complex",
            "UseComplexAsSimple",
            "UseSimpleAsComplex"
          ]
        },
        "hullCount": {
          "type": "number",
          "description": "Number of convex hulls for decomposition."
        },
        "hullPrecision": {
          "type": "number",
          "description": "Precision for convex hull generation (0-1)."
        },
        "maxVerticesPerHull": {
          "type": "number",
          "description": "Maximum vertices per convex hull."
        },
        "lodCount": {
          "type": "number",
          "description": "Number of LOD levels to generate."
        },
        "lodIndex": {
          "type": "number",
          "description": "Specific LOD index to configure."
        },
        "reductionPercent": {
          "type": "number",
          "description": "Percent of triangles to reduce per LOD."
        },
        "screenSize": {
          "type": "number",
          "description": "Screen size threshold for LOD switching."
        },
        "screenSizes": {
          "type": "array",
          "description": "Array of screen sizes for each LOD.",
          "items": {
            "type": "number"
          }
        },
        "preserveBorders": {
          "type": "boolean",
          "description": "Preserve mesh borders during LOD generation."
        },
        "preserveUVs": {
          "type": "boolean",
          "description": "Preserve UV seams during LOD generation."
        },
        "exportFormat": {
          "type": "string",
          "description": "Export file format.",
          "enum": [
            "FBX",
            "OBJ",
            "glTF",
            "USD"
          ]
        },
        "exportPath": {
          "type": "string",
          "description": "Export file path."
        },
        "includeNormals": {
          "type": "boolean",
          "description": "Include normals in export."
        },
        "includeUVs": {
          "type": "boolean",
          "description": "Include UVs in export."
        },
        "includeTangents": {
          "type": "boolean",
          "description": "Include tangents in export."
        },
        "createAsset": {
          "type": "boolean",
          "description": "Create as persistent asset."
        },
        "overwrite": {
          "type": "boolean",
          "description": "Overwrite if the asset/file already exists."
        },
        "save": {
          "type": "boolean",
          "description": "Save the asset(s) after the operation."
        },
        "enableNanite": {
          "type": "boolean",
          "description": "Enable Nanite for the output mesh."
        }
      },
      "required": [
        "action"
      ]
    }
  },
  {
    "name": "manage_input",
    "description": "Create Input Actions and Mapping Contexts. Add key/gamepad bindings with modifiers and triggers.",
    "category": "utility",
    "inputSchema": {
      "type": "object",
      "properties": {
        "action": {
          "type": "string",
          "description": "Action to perform",
          "enum": [
            "create_input_action",
            "create_input_mapping_context",
            "add_mapping",
            "remove_mapping",
            "map_input_action",
            "set_input_trigger",
            "set_input_modifier",
            "enable_input_mapping",
            "disable_input_action",
            "get_input_info"
          ]
        },
        "name": {
          "type": "string",
          "description": "Name identifier."
        },
        "path": {
          "type": "string",
          "description": "Path to a directory."
        },
        "contextPath": {
          "type": "string",
          "description": "Asset path (e.g., /Game/Path/Asset)."
        },
        "actionPath": {
          "type": "string",
          "description": "Asset path (e.g., /Game/Path/Asset)."
        },
        "key": {
          "type": "string"
        },
        "triggerType": {
          "type": "string"
        },
        "modifierType": {
          "type": "string"
        },
        "assetPath": {
          "type": "string",
          "description": "Asset path (e.g., /Game/Path/Asset)."
        },
        "priority": {
          "type": "number",
          "description": "Priority for input mapping context (default: 0)."
        }
      },
      "required": [
        "action"
      ]
    }
  },
  {
    "name": "manage_interaction",
    "description": "Create interactive objects: doors, switches, chests, levers. Set up destructible meshes and trigger volumes.",
    "category": "gameplay",
    "inputSchema": {
      "type": "object",
      "properties": {
        "action": {
          "type": "string",
          "description": "The interaction action to perform.",
          "enum": [
            "create_interaction_component",
            "configure_interaction_trace",
            "configure_interaction_widget",
            "add_interaction_events",
            "create_interactable_interface",
            "create_door_actor",
            "configure_door_properties",
            "create_switch_actor",
            "configure_switch_properties",
            "create_chest_actor",
            "configure_chest_properties",
            "create_lever_actor",
            "setup_destructible_mesh",
            "configure_destruction_levels",
            "configure_destruction_effects",
            "configure_destruction_damage",
            "add_destruction_component",
            "create_trigger_actor",
            "configure_trigger_events",
            "configure_trigger_filter",
            "configure_trigger_response",
            "get_interaction_info"
          ]
        },
        "name": {
          "type": "string",
          "description": "Name identifier."
        },
        "folder": {
          "type": "string",
          "description": "Path to a directory."
        },
        "blueprintPath": {
          "type": "string",
          "description": "Blueprint asset path."
        },
        "actorName": {
          "type": "string",
          "description": "Name of the actor."
        },
        "componentName": {
          "type": "string",
          "description": "Name of the component."
        },
        "traceType": {
          "type": "string",
          "description": "Type of interaction trace.",
          "enum": [
            "line",
            "sphere",
            "box"
          ]
        },
        "traceChannel": {
          "type": "string",
          "description": "Collision trace channel."
        },
        "traceDistance": {
          "type": "number",
          "description": "Trace distance."
        },
        "traceRadius": {
          "type": "number",
          "description": "Trace radius."
        },
        "traceFrequency": {
          "type": "number",
          "description": "Trace frequency."
        },
        "widgetClass": {
          "type": "string",
          "description": "Widget class path."
        },
        "widgetOffset": {
          "type": "object",
          "description": "Widget offset from actor.",
          "properties": {
            "x": {
              "type": "number"
            },
            "y": {
              "type": "number"
            },
            "z": {
              "type": "number"
            }
          }
        },
        "showOnHover": {
          "type": "boolean",
          "description": "Show widget when hovering."
        },
        "showPromptText": {
          "type": "boolean",
          "description": "Show interaction prompt text."
        },
        "promptTextFormat": {
          "type": "string",
          "description": "Format string for prompt (e.g., \"Press {Key} to {Action}\")."
        },
        "doorPath": {
          "type": "string",
          "description": "Path to door actor blueprint."
        },
        "meshPath": {
          "type": "string",
          "description": "Mesh asset path."
        },
        "openAngle": {
          "type": "number",
          "description": "Door open rotation angle in degrees."
        },
        "openTime": {
          "type": "number",
          "description": "Time to open/close door in seconds."
        },
        "openDirection": {
          "type": "string",
          "description": "Door open direction.",
          "enum": [
            "push",
            "pull",
            "auto"
          ]
        },
        "pivotOffset": {
          "type": "object",
          "description": "Offset for door pivot point.",
          "properties": {
            "x": {
              "type": "number"
            },
            "y": {
              "type": "number"
            },
            "z": {
              "type": "number"
            }
          }
        },
        "locked": {
          "type": "boolean",
          "description": "Whether the item is locked."
        },
        "keyItemPath": {
          "type": "string",
          "description": "Item required to unlock."
        },
        "openSound": {
          "type": "string",
          "description": "Sound to play on open."
        },
        "closeSound": {
          "type": "string",
          "description": "Sound to play on close."
        },
        "autoClose": {
          "type": "boolean",
          "description": "Automatically close after opening."
        },
        "autoCloseDelay": {
          "type": "number",
          "description": "Delay before auto-close in seconds."
        },
        "requiresKey": {
          "type": "boolean",
          "description": "Whether interaction requires a key item."
        },
        "switchPath": {
          "type": "string",
          "description": "Path to switch actor blueprint."
        },
        "switchType": {
          "type": "string",
          "description": "Type of switch.",
          "enum": [
            "button",
            "lever",
            "pressure_plate",
            "toggle"
          ]
        },
        "toggleable": {
          "type": "boolean",
          "description": "Whether switch can be toggled."
        },
        "oneShot": {
          "type": "boolean",
          "description": "Whether switch can only be used once."
        },
        "resetTime": {
          "type": "number",
          "description": "Time to reset switch in seconds."
        },
        "activateSound": {
          "type": "string",
          "description": "Sound on activation."
        },
        "deactivateSound": {
          "type": "string",
          "description": "Sound on deactivation."
        },
        "targetActors": {
          "type": "array",
          "description": "Actors affected by this switch.",
          "items": {
            "type": "string"
          }
        },
        "chestPath": {
          "type": "string",
          "description": "Path to chest actor blueprint."
        },
        "lidMeshPath": {
          "type": "string",
          "description": "Path to lid mesh."
        },
        "lootTablePath": {
          "type": "string",
          "description": "Path to loot table asset."
        },
        "respawnable": {
          "type": "boolean"
        },
        "respawnTime": {
          "type": "number",
          "description": "Respawn time in seconds."
        },
        "leverType": {
          "type": "string",
          "description": "Lever movement type.",
          "enum": [
            "rotate",
            "translate"
          ]
        },
        "moveDistance": {
          "type": "number",
          "description": "Distance for translation lever."
        },
        "moveTime": {
          "type": "number",
          "description": "Time for lever movement."
        },
        "fractureMode": {
          "type": "string",
          "description": "Fracture pattern type.",
          "enum": [
            "voronoi",
            "uniform",
            "radial",
            "custom"
          ]
        },
        "fracturePieces": {
          "type": "number",
          "description": "Number of fracture pieces."
        },
        "enablePhysics": {
          "type": "boolean",
          "description": "Enable physics on destruction."
        },
        "levels": {
          "type": "array",
          "description": "Destruction level definitions.",
          "items": {
            "type": "object"
          }
        },
        "destroySound": {
          "type": "string",
          "description": "Sound on destruction."
        },
        "destroyParticle": {
          "type": "string",
          "description": "Particle effect on destruction."
        },
        "debrisPhysicsMaterial": {
          "type": "string",
          "description": "Physics material for debris."
        },
        "debrisLifetime": {
          "type": "number",
          "description": "Debris lifetime in seconds."
        },
        "maxHealth": {
          "type": "number",
          "description": "Maximum health before destruction."
        },
        "damageThresholds": {
          "type": "array",
          "description": "Damage thresholds for destruction levels.",
          "items": {
            "type": "number"
          }
        },
        "impactDamageMultiplier": {
          "type": "number",
          "description": "Multiplier for impact damage."
        },
        "radialDamageMultiplier": {
          "type": "number",
          "description": "Multiplier for radial damage."
        },
        "autoDestroy": {
          "type": "boolean",
          "description": "Automatically destroy at zero health."
        },
        "triggerPath": {
          "type": "string",
          "description": "Path to trigger actor blueprint."
        },
        "triggerShape": {
          "type": "string",
          "description": "Shape of trigger volume.",
          "enum": [
            "box",
            "sphere",
            "capsule"
          ]
        },
        "size": {
          "type": "object",
          "description": "Size of trigger volume.",
          "properties": {
            "x": {
              "type": "number"
            },
            "y": {
              "type": "number"
            },
            "z": {
              "type": "number"
            }
          }
        },
        "filterByTag": {
          "type": "string",
          "description": "Actor tag filter for trigger."
        },
        "filterByClass": {
          "type": "string",
          "description": "Actor class filter for trigger."
        },
        "filterByInterface": {
          "type": "string",
          "description": "Interface filter for trigger."
        },
        "ignoreClasses": {
          "type": "array",
          "description": "Classes to ignore in trigger.",
          "items": {
            "type": "string"
          }
        },
        "ignoreTags": {
          "type": "array",
          "description": "Tags to ignore in trigger.",
          "items": {
            "type": "string"
          }
        },
        "onEnterEvent": {
          "type": "string",
          "description": "Event dispatcher name for enter."
        },
        "onExitEvent": {
          "type": "string",
          "description": "Event dispatcher name for exit."
        },
        "onStayEvent": {
          "type": "string",
          "description": "Event dispatcher name for stay."
        },
        "stayInterval": {
          "type": "number",
          "description": "Interval for stay events in seconds."
        },
        "responseType": {
          "type": "string",
          "description": "How trigger responds.",
          "enum": [
            "once",
            "repeatable",
            "toggle"
          ]
        },
        "cooldown": {
          "type": "number",
          "description": "Cooldown time in seconds."
        },
        "maxActivations": {
          "type": "number",
          "description": "Maximum number of activations (0 = unlimited)."
        }
      },
      "required": [
        "action"
      ]
    }
  },
  {
    "name": "manage_inventory",
    "description": "Create item data assets, inventory components, world pickups, loot tables, and crafting recipes.",
    "category": "gameplay",
    "inputSchema": {
      "type": "object",
      "properties": {
        "action": {
          "type": "string",
          "description": "Inventory action to perform.",
          "enum": [
            "create_item_data_asset",
            "set_item_properties",
            "create_item_category",
            "assign_item_category",
            "create_inventory_component",
            "configure_inventory_slots",
            "add_inventory_functions",
            "configure_inventory_events",
            "set_inventory_replication",
            "create_pickup_actor",
            "configure_pickup_interaction",
            "configure_pickup_respawn",
            "configure_pickup_effects",
            "create_equipment_component",
            "define_equipment_slots",
            "configure_equipment_effects",
            "add_equipment_functions",
            "configure_equipment_visuals",
            "create_loot_table",
            "add_loot_entry",
            "configure_loot_drop",
            "set_loot_quality_tiers",
            "create_crafting_recipe",
            "configure_recipe_requirements",
            "create_crafting_station",
            "add_crafting_component",
            "get_inventory_info"
          ]
        },
        "name": {
          "type": "string",
          "description": "Name of the asset to create."
        },
        "path": {
          "type": "string",
          "description": "Directory path for asset creation."
        },
        "folder": {
          "type": "string",
          "description": "Path to a directory."
        },
        "save": {
          "type": "boolean",
          "description": "Save the asset(s) after the operation."
        },
        "blueprintPath": {
          "type": "string",
          "description": "Blueprint asset path."
        },
        "itemPath": {
          "type": "string",
          "description": "Path to item data asset."
        },
        "parentClass": {
          "type": "string",
          "description": "Path or name of the parent class."
        },
        "displayName": {
          "type": "string"
        },
        "description": {
          "type": "string"
        },
        "icon": {
          "type": "string",
          "description": "Path to icon texture."
        },
        "mesh": {
          "type": "string",
          "description": "Path to mesh asset."
        },
        "stackSize": {
          "type": "number"
        },
        "weight": {
          "type": "number"
        },
        "rarity": {
          "type": "string",
          "description": "Item rarity tier.",
          "enum": [
            "Common",
            "Uncommon",
            "Rare",
            "Epic",
            "Legendary",
            "Custom"
          ]
        },
        "value": {
          "type": "number"
        },
        "tags": {
          "type": "array",
          "description": "Gameplay tags for item categorization.",
          "items": {
            "type": "string"
          }
        },
        "customProperties": {
          "type": "object",
          "description": "Custom key-value properties for item.",
          "additionalProperties": true
        },
        "categoryPath": {
          "type": "string",
          "description": "Path to item category asset."
        },
        "parentCategory": {
          "type": "string",
          "description": "Parent category path."
        },
        "categoryIcon": {
          "type": "string",
          "description": "Icon texture for category."
        },
        "componentName": {
          "type": "string",
          "description": "Name of the component."
        },
        "slotCount": {
          "type": "number"
        },
        "slotSize": {
          "type": "object",
          "description": "Size of each slot (for grid inventory).",
          "properties": {
            "width": {
              "type": "number"
            },
            "height": {
              "type": "number"
            }
          }
        },
        "maxWeight": {
          "type": "number"
        },
        "allowStacking": {
          "type": "boolean",
          "description": "Allow items to stack."
        },
        "slotCategories": {
          "type": "array",
          "description": "Allowed item categories per slot.",
          "items": {
            "type": "string"
          }
        },
        "slotRestrictions": {
          "type": "array",
          "description": "Per-slot category restrictions.",
          "items": {
            "type": "object"
          }
        },
        "replicated": {
          "type": "boolean",
          "description": "Whether to replicate."
        },
        "replicationCondition": {
          "type": "string",
          "description": "Replication condition for inventory.",
          "enum": [
            "None",
            "OwnerOnly",
            "SkipOwner",
            "SimulatedOnly",
            "AutonomousOnly",
            "Custom"
          ]
        },
        "pickupPath": {
          "type": "string",
          "description": "Path to pickup actor Blueprint."
        },
        "meshPath": {
          "type": "string",
          "description": "Mesh asset path."
        },
        "itemDataPath": {
          "type": "string",
          "description": "Path to item data asset."
        },
        "interactionRadius": {
          "type": "number",
          "description": "Radius for pickup interaction."
        },
        "interactionType": {
          "type": "string",
          "description": "How player picks up item.",
          "enum": [
            "Overlap",
            "Interact",
            "Key",
            "Hold"
          ]
        },
        "interactionKey": {
          "type": "string",
          "description": "Input action for pickup (if type is Key/Hold)."
        },
        "prompt": {
          "type": "string",
          "description": "Prompt text."
        },
        "highlightMaterial": {
          "type": "string",
          "description": "Material for highlight effect."
        },
        "respawnable": {
          "type": "boolean"
        },
        "respawnTime": {
          "type": "number",
          "description": "Respawn time in seconds."
        },
        "respawnEffect": {
          "type": "string",
          "description": "Niagara effect for respawn."
        },
        "pickupSound": {
          "type": "string",
          "description": "Sound cue for pickup."
        },
        "pickupParticle": {
          "type": "string",
          "description": "Particle effect on pickup."
        },
        "bobbing": {
          "type": "boolean",
          "description": "Enable bobbing animation."
        },
        "rotation": {
          "type": "boolean",
          "description": "Enable rotation animation."
        },
        "glowEffect": {
          "type": "boolean",
          "description": "Enable glow effect."
        },
        "slots": {
          "type": "array",
          "description": "Equipment slot definitions.",
          "items": {
            "type": "object"
          }
        },
        "statModifiers": {
          "type": "array",
          "description": "Stat modifiers when equipped.",
          "items": {
            "type": "object"
          }
        },
        "abilityGrants": {
          "type": "array",
          "description": "Gameplay abilities granted when equipped.",
          "items": {
            "type": "string"
          }
        },
        "passiveEffects": {
          "type": "array",
          "description": "Passive gameplay effects when equipped.",
          "items": {
            "type": "string"
          }
        },
        "attachToSocket": {
          "type": "boolean",
          "description": "Attach mesh to socket when equipped."
        },
        "meshComponent": {
          "type": "string",
          "description": "Component name for equipment mesh."
        },
        "animationOverrides": {
          "type": "object",
          "description": "Animation overrides (slot -> anim asset).",
          "additionalProperties": true
        },
        "lootTablePath": {
          "type": "string",
          "description": "Path to loot table asset."
        },
        "lootWeight": {
          "type": "number",
          "description": "Weight for drop chance calculation."
        },
        "minQuantity": {
          "type": "number",
          "description": "Minimum drop quantity."
        },
        "maxQuantity": {
          "type": "number",
          "description": "Maximum drop quantity."
        },
        "conditions": {
          "type": "array",
          "description": "Conditions for loot entry (gameplay tag expressions).",
          "items": {
            "type": "string"
          }
        },
        "actorPath": {
          "type": "string",
          "description": "Path to actor Blueprint for loot drop."
        },
        "dropCount": {
          "type": "number",
          "description": "Number of drops to roll."
        },
        "guaranteedDrops": {
          "type": "array",
          "description": "Item paths that always drop.",
          "items": {
            "type": "string"
          }
        },
        "dropRadius": {
          "type": "number",
          "description": "Radius for scattered drops."
        },
        "dropForce": {
          "type": "number",
          "description": "Physics force applied to drops."
        },
        "tiers": {
          "type": "array",
          "description": "Quality tier definitions.",
          "items": {
            "type": "object"
          }
        },
        "recipePath": {
          "type": "string",
          "description": "Path to crafting recipe asset."
        },
        "outputItemPath": {
          "type": "string",
          "description": "Path to item produced by recipe."
        },
        "outputQuantity": {
          "type": "number",
          "description": "Quantity produced."
        },
        "ingredients": {
          "type": "array",
          "description": "Required ingredients with quantities.",
          "items": {
            "type": "object"
          }
        },
        "craftTime": {
          "type": "number",
          "description": "Time in seconds to craft."
        },
        "requiredLevel": {
          "type": "number",
          "description": "Required player level."
        },
        "requiredSkills": {
          "type": "array",
          "description": "Required skill tags.",
          "items": {
            "type": "string"
          }
        },
        "requiredStation": {
          "type": "string",
          "description": "Required crafting station type."
        },
        "unlockConditions": {
          "type": "array",
          "description": "Conditions to unlock recipe.",
          "items": {
            "type": "string"
          }
        },
        "recipes": {
          "type": "array",
          "description": "Recipe paths for crafting station.",
          "items": {
            "type": "string"
          }
        },
        "stationType": {
          "type": "string",
          "description": "Type of crafting station."
        }
      },
      "required": [
        "action"
      ]
    }
  },
  {
    "name": "manage_level",
    "description": "Load/save levels, configure streaming, manage World Partition cells, and build lighting.",
    "category": "core",
    "inputSchema": {
      "type": "object",
      "properties": {
        "action": {
          "type": "string",
          "description": "Action",
          "enum": [
            "load",
            "save",
            "save_as",
            "save_level_as",
            "stream",
            "unload",
            "create_level",
            "create_light",
            "build_lighting",
            "set_metadata",
            "load_cells",
            "set_datalayer",
            "create_datalayer",
            "export_level",
            "import_level",
            "list_levels",
            "get_summary",
            "delete",
            "delete_level",
            "validate_level",
            "cleanup_invalid_datalayers",
            "add_sublevel",
            "rename_level",
            "duplicate_level",
            "get_current_level"
          ]
        },
        "levelPath": {
          "type": "string",
          "description": "Level asset path."
        },
        "levelPaths": {
          "type": "array",
          "items": {
            "type": "string"
          }
        },
        "levelName": {
          "type": "string"
        },
        "path": {
          "type": "string",
          "description": "Directory path for asset creation."
        },
        "savePath": {
          "type": "string",
          "description": "Path to save the asset."
        },
        "destinationPath": {
          "type": "string",
          "description": "Destination path for move/copy."
        },
        "targetPath": {
          "type": "string",
          "description": "Path to a directory."
        },
        "exportPath": {
          "type": "string",
          "description": "Export file path."
        },
        "packagePath": {
          "type": "string",
          "description": "Path to a directory."
        },
        "sourcePath": {
          "type": "string",
          "description": "Source path for import/move/copy."
        },
        "sublevelPath": {
          "type": "string",
          "description": "Level asset path."
        },
        "parentLevel": {
          "type": "string",
          "description": "Parent level path."
        },
        "parentPath": {
          "type": "string",
          "description": "Path to a directory."
        },
        "streamingMethod": {
          "type": "string"
        },
        "streaming": {
          "type": "boolean"
        },
        "shouldBeLoaded": {
          "type": "boolean"
        },
        "shouldBeVisible": {
          "type": "boolean"
        },
        "lightType": {
          "type": "string",
          "description": "Light type. Accepts short names (Point), class names (PointLight), or lowercase (point).",
          "enum": [
            "Directional",
            "Point",
            "Spot",
            "Rect",
            "DirectionalLight",
            "PointLight",
            "SpotLight",
            "RectLight",
            "directional",
            "point",
            "spot",
            "rect"
          ]
        },
        "intensity": {
          "type": "number"
        },
        "color": {
          "type": "array",
          "description": "RGBA color as an array [r, g, b, a].",
          "items": {
            "type": "number"
          }
        },
        "location": {
          "type": "object",
          "description": "3D location (x, y, z).",
          "properties": {
            "x": {
              "type": "number"
            },
            "y": {
              "type": "number"
            },
            "z": {
              "type": "number"
            }
          }
        },
        "rotation": {
          "type": "object",
          "description": "3D rotation (pitch, yaw, roll).",
          "properties": {
            "pitch": {
              "type": "number"
            },
            "yaw": {
              "type": "number"
            },
            "roll": {
              "type": "number"
            }
          }
        },
        "cells": {
          "type": "array",
          "items": {
            "type": "string"
          }
        },
        "dataLayerName": {
          "type": "string",
          "description": "Name of the data layer."
        },
        "dataLayerLabel": {
          "type": "string"
        },
        "dataLayerState": {
          "type": "string"
        },
        "actorPath": {
          "type": "string",
          "description": "Path to actor."
        },
        "min": {
          "type": "object",
          "description": "3D location (x, y, z).",
          "properties": {
            "x": {
              "type": "number"
            },
            "y": {
              "type": "number"
            },
            "z": {
              "type": "number"
            }
          }
        },
        "max": {
          "type": "object",
          "description": "3D location (x, y, z).",
          "properties": {
            "x": {
              "type": "number"
            },
            "y": {
              "type": "number"
            },
            "z": {
              "type": "number"
            }
          }
        },
        "origin": {
          "type": "object",
          "description": "3D location (x, y, z).",
          "properties": {
            "x": {
              "type": "number"
            },
            "y": {
              "type": "number"
            },
            "z": {
              "type": "number"
            }
          }
        },
        "extent": {
          "type": "object",
          "description": "3D extent (half-size).",
          "properties": {
            "x": {
              "type": "number"
            },
            "y": {
              "type": "number"
            },
            "z": {
              "type": "number"
            }
          }
        },
        "template": {
          "type": "string"
        },
        "useWorldPartition": {
          "type": "boolean"
        },
        "metadata": {
          "type": "object",
          "additionalProperties": true
        },
        "newName": {
          "type": "string"
        },
        "timeoutMs": {
          "type": "number"
        }
      },
      "required": [
        "action"
      ]
    }
  },
  {
    "name": "manage_level_structure",
    "description": "Create levels and sublevels. Configure World Partition, streaming, data layers, HLOD, and level instances.",
    "category": "world",
    "inputSchema": {
      "type": "object",
      "properties": {
        "action": {
          "type": "string",
          "description": "Level structure action to perform.",
          "enum": [
            "create_level",
            "create_sublevel",
            "configure_level_streaming",
            "set_streaming_distance",
            "configure_level_bounds",
            "enable_world_partition",
            "configure_grid_size",
            "create_data_layer",
            "assign_actor_to_data_layer",
            "configure_hlod_layer",
            "create_minimap_volume",
            "open_level_blueprint",
            "add_level_blueprint_node",
            "connect_level_blueprint_nodes",
            "create_level_instance",
            "create_packed_level_actor",
            "get_level_structure_info"
          ]
        },
        "levelName": {
          "type": "string"
        },
        "levelPath": {
          "type": "string",
          "description": "Level asset path."
        },
        "parentLevel": {
          "type": "string",
          "description": "Parent level path."
        },
        "templateLevel": {
          "type": "string",
          "description": "Template level path."
        },
        "bCreateWorldPartition": {
          "type": "boolean",
          "description": "Create with World Partition enabled."
        },
        "bUseExternalActors": {
          "type": "boolean",
          "description": "Enable One File Per Actor (OFPA/External Actors) for Data Layer compatibility. Automatically enabled when bCreateWorldPartition is true."
        },
        "sublevelName": {
          "type": "string",
          "description": "Name of the sublevel."
        },
        "sublevelPath": {
          "type": "string",
          "description": "Level asset path."
        },
        "streamingMethod": {
          "type": "string",
          "description": "Level streaming method.",
          "enum": [
            "Blueprint",
            "AlwaysLoaded",
            "Disabled"
          ]
        },
        "bShouldBeVisible": {
          "type": "boolean",
          "description": "Level should be visible when loaded."
        },
        "bShouldBlockOnLoad": {
          "type": "boolean",
          "description": "Block game until level is loaded."
        },
        "bDisableDistanceStreaming": {
          "type": "boolean",
          "description": "Disable distance-based streaming."
        },
        "streamingDistance": {
          "type": "number",
          "description": "Distance/radius for streaming volume (creates ALevelStreamingVolume)."
        },
        "streamingUsage": {
          "type": "string",
          "description": "Streaming volume usage mode (default: LoadingAndVisibility).",
          "enum": [
            "Loading",
            "LoadingAndVisibility",
            "VisibilityBlockingOnLoad",
            "BlockingOnLoad",
            "LoadingNotVisible"
          ]
        },
        "createVolume": {
          "type": "boolean",
          "description": "Create a streaming volume (true) or just report existing volumes (false). Default: true."
        },
        "boundsOrigin": {
          "type": "object",
          "description": "Origin of level bounds.",
          "properties": {
            "x": {
              "type": "number"
            },
            "y": {
              "type": "number"
            },
            "z": {
              "type": "number"
            }
          }
        },
        "boundsExtent": {
          "type": "object",
          "description": "Extent of level bounds.",
          "properties": {
            "x": {
              "type": "number"
            },
            "y": {
              "type": "number"
            },
            "z": {
              "type": "number"
            }
          }
        },
        "bAutoCalculateBounds": {
          "type": "boolean",
          "description": "Auto-calculate bounds from content."
        },
        "bEnableWorldPartition": {
          "type": "boolean",
          "description": "Enable World Partition for level."
        },
        "gridCellSize": {
          "type": "number",
          "description": "World Partition grid cell size."
        },
        "loadingRange": {
          "type": "number",
          "description": "Loading range for grid cells."
        },
        "dataLayerName": {
          "type": "string",
          "description": "Name of the data layer."
        },
        "dataLayerLabel": {
          "type": "string",
          "description": "Display label for the data layer."
        },
        "bIsInitiallyVisible": {
          "type": "boolean",
          "description": "Data layer initially visible."
        },
        "bIsInitiallyLoaded": {
          "type": "boolean",
          "description": "Data layer initially loaded."
        },
        "dataLayerType": {
          "type": "string",
          "description": "Type of data layer.",
          "enum": [
            "Runtime",
            "Editor"
          ]
        },
        "actorName": {
          "type": "string",
          "description": "Name of the actor."
        },
        "actorPath": {
          "type": "string",
          "description": "Path to actor."
        },
        "hlodLayerName": {
          "type": "string",
          "description": "Name of the HLOD layer."
        },
        "hlodLayerPath": {
          "type": "string",
          "description": "Path to HLOD layer."
        },
        "bIsSpatiallyLoaded": {
          "type": "boolean",
          "description": "HLOD is spatially loaded."
        },
        "cellSize": {
          "type": "number",
          "description": "HLOD cell size."
        },
        "loadingDistance": {
          "type": "number",
          "description": "HLOD loading distance."
        },
        "volumeName": {
          "type": "string",
          "description": "Name of the volume."
        },
        "volumeLocation": {
          "type": "object",
          "description": "Location of the volume.",
          "properties": {
            "x": {
              "type": "number"
            },
            "y": {
              "type": "number"
            },
            "z": {
              "type": "number"
            }
          }
        },
        "volumeExtent": {
          "type": "object",
          "description": "Extent of the volume.",
          "properties": {
            "x": {
              "type": "number"
            },
            "y": {
              "type": "number"
            },
            "z": {
              "type": "number"
            }
          }
        },
        "nodeClass": {
          "type": "string",
          "description": "Node class path."
        },
        "nodePosition": {
          "type": "object",
          "description": "Position of node in graph.",
          "properties": {
            "x": {
              "type": "number"
            },
            "y": {
              "type": "number"
            }
          }
        },
        "nodeName": {
          "type": "string",
          "description": "Name of the node."
        },
        "sourceNodeName": {
          "type": "string",
          "description": "Source node name."
        },
        "sourcePinName": {
          "type": "string",
          "description": "Name of the source pin."
        },
        "targetNodeName": {
          "type": "string",
          "description": "Target node name."
        },
        "targetPinName": {
          "type": "string",
          "description": "Name of the target pin."
        },
        "levelInstanceName": {
          "type": "string",
          "description": "Level instance name."
        },
        "levelAssetPath": {
          "type": "string",
          "description": "Path to the level asset for instancing."
        },
        "instanceLocation": {
          "type": "object",
          "description": "Location of the level instance.",
          "properties": {
            "x": {
              "type": "number"
            },
            "y": {
              "type": "number"
            },
            "z": {
              "type": "number"
            }
          }
        },
        "instanceRotation": {
          "type": "object",
          "description": "Rotation of the level instance.",
          "properties": {
            "pitch": {
              "type": "number"
            },
            "yaw": {
              "type": "number"
            },
            "roll": {
              "type": "number"
            }
          }
        },
        "instanceScale": {
          "type": "object",
          "description": "Scale of the level instance.",
          "properties": {
            "x": {
              "type": "number"
            },
            "y": {
              "type": "number"
            },
            "z": {
              "type": "number"
            }
          }
        },
        "packedLevelName": {
          "type": "string",
          "description": "Name for the packed level actor."
        },
        "bPackBlueprints": {
          "type": "boolean",
          "description": "Include blueprints in packed level."
        },
        "bPackStaticMeshes": {
          "type": "boolean",
          "description": "Include static meshes in packed level."
        },
        "save": {
          "type": "boolean",
          "description": "Save the asset(s) after the operation."
        }
      },
      "required": [
        "action"
      ]
    }
  },
  {
    "name": "manage_lighting",
    "description": "Spawn lights (point, spot, rect, sky), configure GI, shadows, volumetric fog, and build lighting.",
    "category": "world",
    "inputSchema": {
      "type": "object",
      "properties": {
        "action": {
          "type": "string",
          "description": "Action",
          "enum": [
            "spawn_light",
            "create_light",
            "spawn_sky_light",
            "create_sky_light",
            "ensure_single_sky_light",
            "create_lightmass_volume",
            "create_lighting_enabled_level",
            "create_dynamic_light",
            "setup_global_illumination",
            "configure_shadows",
            "set_exposure",
            "set_ambient_occlusion",
            "setup_volumetric_fog",
            "build_lighting",
            "list_light_types"
          ]
        },
        "name": {
          "type": "string",
          "description": "Name identifier."
        },
        "location": {
          "type": "object",
          "description": "3D location (x, y, z).",
          "properties": {
            "x": {
              "type": "number"
            },
            "y": {
              "type": "number"
            },
            "z": {
              "type": "number"
            }
          }
        },
        "rotation": {
          "type": "object",
          "description": "3D rotation (pitch, yaw, roll).",
          "properties": {
            "pitch": {
              "type": "number"
            },
            "yaw": {
              "type": "number"
            },
            "roll": {
              "type": "number"
            }
          }
        },
        "lightType": {
          "type": "string",
          "description": "Light type. Accepts short names (Point), class names (PointLight), or lowercase (point).",
          "enum": [
            "Directional",
            "Point",
            "Spot",
            "Rect",
            "DirectionalLight",
            "PointLight",
            "SpotLight",
            "RectLight",
            "directional",
            "point",
            "spot",
            "rect"
          ]
        },
        "lightClass": {
          "type": "string",
          "description": "Unreal light class name (e.g., PointLight, SpotLight). Alternative to lightType."
        },
        "intensity": {
          "type": "number"
        },
        "color": {
          "type": "array",
          "description": "RGBA color as an array [r, g, b, a].",
          "items": {
            "type": "number"
          }
        },
        "castShadows": {
          "type": "boolean"
        },
        "useAsAtmosphereSunLight": {
          "type": "boolean",
          "description": "For Directional Lights, use as Atmosphere Sun Light."
        },
        "temperature": {
          "type": "number"
        },
        "radius": {
          "type": "number"
        },
        "falloffExponent": {
          "type": "number"
        },
        "innerCone": {
          "type": "number"
        },
        "outerCone": {
          "type": "number"
        },
        "width": {
          "type": "number"
        },
        "height": {
          "type": "number"
        },
        "sourceType": {
          "type": "string",
          "enum": [
            "CapturedScene",
            "SpecifiedCubemap"
          ]
        },
        "cubemapPath": {
          "type": "string",
          "description": "Texture asset path."
        },
        "recapture": {
          "type": "boolean"
        },
        "method": {
          "type": "string",
          "enum": [
            "Lightmass",
            "LumenGI",
            "ScreenSpace",
            "None"
          ]
        },
        "quality": {
          "type": "string"
        },
        "indirectLightingIntensity": {
          "type": "number"
        },
        "bounces": {
          "type": "number"
        },
        "shadowQuality": {
          "type": "string"
        },
        "cascadedShadows": {
          "type": "boolean"
        },
        "shadowDistance": {
          "type": "number"
        },
        "contactShadows": {
          "type": "boolean"
        },
        "rayTracedShadows": {
          "type": "boolean"
        },
        "compensationValue": {
          "type": "number"
        },
        "minBrightness": {
          "type": "number"
        },
        "maxBrightness": {
          "type": "number"
        },
        "enabled": {
          "type": "boolean",
          "description": "Whether the item/feature is enabled."
        },
        "density": {
          "type": "number"
        },
        "scatteringIntensity": {
          "type": "number"
        },
        "fogHeight": {
          "type": "number"
        },
        "buildOnlySelected": {
          "type": "boolean"
        },
        "buildReflectionCaptures": {
          "type": "boolean"
        },
        "levelName": {
          "type": "string"
        },
        "copyActors": {
          "type": "boolean"
        },
        "useTemplate": {
          "type": "boolean"
        },
        "size": {
          "type": "object",
          "description": "3D scale (x, y, z).",
          "properties": {
            "x": {
              "type": "number"
            },
            "y": {
              "type": "number"
            },
            "z": {
              "type": "number"
            }
          }
        }
      },
      "required": [
        "action"
      ]
    }
  },
  {
    "name": "manage_material_authoring",
    "description": "Create materials with expressions, parameters, functions, instances, and landscape blend layers.",
    "category": "authoring",
    "inputSchema": {
      "type": "object",
      "properties": {
        "subAction": {
          "type": "string",
          "description": "Canonical material authoring sub-action to perform.",
          "enum": [
            "create_material",
            "set_blend_mode",
            "set_shading_model",
            "set_material_domain",
            "add_texture_sample",
            "add_texture_coordinate",
            "add_scalar_parameter",
            "add_vector_parameter",
            "add_static_switch_parameter",
            "add_math_node",
            "add_world_position",
            "add_vertex_normal",
            "add_pixel_depth",
            "add_fresnel",
            "add_reflection_vector",
            "add_panner",
            "add_rotator",
            "add_noise",
            "add_voronoi",
            "add_if",
            "add_switch",
            "add_custom_expression",
            "connect_nodes",
            "disconnect_nodes",
            "create_material_function",
            "add_function_input",
            "add_function_output",
            "use_material_function",
            "create_material_instance",
            "set_scalar_parameter_value",
            "set_vector_parameter_value",
            "set_texture_parameter_value",
            "create_landscape_material",
            "create_decal_material",
            "create_post_process_material",
            "add_landscape_layer",
            "configure_layer_blend",
            "compile_material",
            "get_material_info",
            "add_material_node",
            "update_function_input",
            "update_function_output",
            "add_material_function_call",
            "update_material_function_call",
            "add_texture_object",
            "add_texture_object_parameter",
            "add_texture_sample_parameter",
            "disconnect_input_pin",
            "remove_material_node",
            "move_material_node",
            "set_material_attributes_mode",
            "get_material_instance_info",
            "set_material_instance_parent",
            "get_material_instance_parameters",
            "set_material_instance_parameter",
            "reset_material_instance_parameter",
            "clear_material_instance_parameters",
            "bulk_set_material_instance_parameters",
            "create_material_function_instance",
            "get_material_function_instance_info",
            "set_material_function_instance_parent",
            "get_material_function_instance_parameters",
            "set_material_function_instance_parameter",
            "reset_material_function_instance_parameter",
            "clear_material_function_instance_parameters",
            "bulk_set_material_function_instance_parameters",
            "get_custom_expression",
            "get_parameter_defaults"
          ]
        },
        "action": {
          "type": "string",
          "description": "Compatibility alias for subAction. New callers should use subAction.",
          "enum": [
            "create_material",
            "set_blend_mode",
            "set_shading_model",
            "set_material_domain",
            "add_texture_sample",
            "add_texture_coordinate",
            "add_scalar_parameter",
            "add_vector_parameter",
            "add_static_switch_parameter",
            "add_math_node",
            "add_world_position",
            "add_vertex_normal",
            "add_pixel_depth",
            "add_fresnel",
            "add_reflection_vector",
            "add_panner",
            "add_rotator",
            "add_noise",
            "add_voronoi",
            "add_if",
            "add_switch",
            "add_custom_expression",
            "connect_nodes",
            "disconnect_nodes",
            "create_material_function",
            "add_function_input",
            "add_function_output",
            "use_material_function",
            "create_material_instance",
            "set_scalar_parameter_value",
            "set_vector_parameter_value",
            "set_texture_parameter_value",
            "create_landscape_material",
            "create_decal_material",
            "create_post_process_material",
            "add_landscape_layer",
            "configure_layer_blend",
            "compile_material",
            "get_material_info",
            "add_material_node",
            "update_function_input",
            "update_function_output",
            "add_material_function_call",
            "update_material_function_call",
            "add_texture_object",
            "add_texture_object_parameter",
            "add_texture_sample_parameter",
            "disconnect_input_pin",
            "remove_material_node",
            "move_material_node",
            "set_material_attributes_mode",
            "get_material_instance_info",
            "set_material_instance_parent",
            "get_material_instance_parameters",
            "set_material_instance_parameter",
            "reset_material_instance_parameter",
            "clear_material_instance_parameters",
            "bulk_set_material_instance_parameters",
            "create_material_function_instance",
            "get_material_function_instance_info",
            "set_material_function_instance_parent",
            "get_material_function_instance_parameters",
            "set_material_function_instance_parameter",
            "reset_material_function_instance_parameter",
            "clear_material_function_instance_parameters",
            "bulk_set_material_function_instance_parameters",
            "get_custom_expression",
            "get_parameter_defaults"
          ]
        },
        "assetPath": {
          "type": "string",
          "description": "Asset path (e.g., /Game/Path/Asset)."
        },
        "name": {
          "type": "string",
          "description": "Name identifier."
        },
        "path": {
          "type": "string",
          "description": "Directory path for asset creation."
        },
        "parentPath": {
          "type": "string",
          "description": "Parent material or material function interface path."
        },
        "parentMaterial": {
          "type": "string",
          "description": "Path to parent material for instances."
        },
        "nodeType": {
          "type": "string",
          "description": "Material expression node type or class name."
        },
        "materialDomain": {
          "type": "string",
          "description": "Material domain type.",
          "enum": [
            "Surface",
            "DeferredDecal",
            "LightFunction",
            "Volume",
            "PostProcess",
            "UI"
          ]
        },
        "blendMode": {
          "type": "string",
          "description": "Blend mode.",
          "enum": [
            "Opaque",
            "Masked",
            "Translucent",
            "Additive",
            "Modulate",
            "AlphaComposite",
            "AlphaHoldout"
          ]
        },
        "shadingModel": {
          "type": "string",
          "description": "Shading model.",
          "enum": [
            "DefaultLit",
            "Unlit",
            "Subsurface",
            "SubsurfaceProfile",
            "PreintegratedSkin",
            "ClearCoat",
            "Hair",
            "Cloth",
            "Eye",
            "TwoSidedFoliage",
            "ThinTranslucent"
          ]
        },
        "twoSided": {
          "type": "boolean",
          "description": "Enable two-sided rendering."
        },
        "x": {
          "type": "number",
          "description": "Node X position."
        },
        "y": {
          "type": "number",
          "description": "Node Y position."
        },
        "texturePath": {
          "type": "string",
          "description": "Texture asset path."
        },
        "runtimeVirtualTexturePath": {
          "type": "string",
          "description": "Runtime virtual texture asset path."
        },
        "sparseVolumeTexturePath": {
          "type": "string",
          "description": "Sparse volume texture asset path."
        },
        "fontPath": {
          "type": "string",
          "description": "Font asset path."
        },
        "fontPage": {
          "type": "integer",
          "description": "Font page index."
        },
        "samplerType": {
          "type": "string",
          "description": "Texture sampler type.",
          "enum": [
            "Color",
            "LinearColor",
            "Normal",
            "Masks",
            "Alpha",
            "VirtualColor",
            "VirtualNormal"
          ]
        },
        "coordinateIndex": {
          "type": "number",
          "description": "UV channel index (0-7)."
        },
        "uTiling": {
          "type": "number",
          "description": "U tiling factor."
        },
        "vTiling": {
          "type": "number",
          "description": "V tiling factor."
        },
        "parameterName": {
          "type": "string",
          "description": "Name of the parameter."
        },
        "parameter": {
          "type": "object",
          "description": "Material parameter identity: name, type, association, and index.",
          "additionalProperties": true
        },
        "defaultValue": {
          "type": "object",
          "description": "Default value for parameter (number for scalar, object for vector, bool for switch).",
          "additionalProperties": true
        },
        "previewValue": {
          "type": "object",
          "description": "Preview value for material function input authoring.",
          "additionalProperties": true
        },
        "group": {
          "type": "string",
          "description": "Group name."
        },
        "sortPriority": {
          "type": "integer",
          "description": "Sort priority for function inputs and parameters."
        },
        "usePreviewValueAsDefault": {
          "type": "boolean",
          "description": "Use preview value as default for function input."
        },
        "blendInputRelevance": {
          "type": "string",
          "description": "Material function input relevance. Authoring currently accepts only General.",
          "enum": [
            "General"
          ]
        },
        "value": {
          "type": "object",
          "description": "Value to set (number, vector object, or texture path).",
          "additionalProperties": true
        },
        "overrides": {
          "type": "array",
          "description": "Bulk parameter override list.",
          "items": {
            "type": "object"
          }
        },
        "operation": {
          "type": "string",
          "description": "Math operation type.",
          "enum": [
            "Add",
            "Subtract",
            "Multiply",
            "Divide",
            "Lerp",
            "Clamp",
            "Power",
            "SquareRoot",
            "Abs",
            "Floor",
            "Ceil",
            "Frac",
            "Sine",
            "Cosine",
            "Saturate",
            "OneMinus",
            "Min",
            "Max",
            "Dot",
            "Cross",
            "Normalize",
            "Append"
          ]
        },
        "constA": {
          "type": "number",
          "description": "Constant A input value."
        },
        "constB": {
          "type": "number",
          "description": "Constant B input value."
        },
        "code": {
          "type": "string",
          "description": "Code or expression."
        },
        "outputType": {
          "type": "string",
          "description": "Output type of custom expression.",
          "enum": [
            "Float1",
            "Float2",
            "Float3",
            "Float4",
            "MaterialAttributes"
          ]
        },
        "description": {
          "type": "string",
          "description": "Description for custom expression or function."
        },
        "sourceNodeId": {
          "type": "string",
          "description": "Source node ID for connection."
        },
        "sourceExpression": {
          "type": "object",
          "description": "ExpressionTarget object for source expression.",
          "additionalProperties": true
        },
        "targetExpression": {
          "type": "object",
          "description": "ExpressionTarget object for target expression.",
          "additionalProperties": true
        },
        "expression": {
          "type": "object",
          "description": "ExpressionTarget object.",
          "additionalProperties": true
        },
        "target": {
          "type": "object",
          "description": "Connection target object.",
          "additionalProperties": true
        },
        "sourceOutputIndex": {
          "type": "integer",
          "description": "Source expression output index."
        },
        "sourceOutputName": {
          "type": "string",
          "description": "Source expression output name."
        },
        "targetInputName": {
          "type": "string",
          "description": "Target expression input name."
        },
        "targetMaterialPin": {
          "type": "string",
          "description": "Main material pin name."
        },
        "sourcePin": {
          "type": "string",
          "description": "Source pin name (output)."
        },
        "targetNodeId": {
          "type": "string",
          "description": "Target node ID for connection."
        },
        "targetPin": {
          "type": "string",
          "description": "Target pin name (input)."
        },
        "nodeId": {
          "type": "string",
          "description": "ID of the node."
        },
        "pinName": {
          "type": "string",
          "description": "Name of the pin."
        },
        "functionPath": {
          "type": "string",
          "description": "Path to function asset."
        },
        "exposeToLibrary": {
          "type": "boolean",
          "description": "Expose function to material library."
        },
        "inputName": {
          "type": "string",
          "description": "Name of the input."
        },
        "outputName": {
          "type": "string",
          "description": "Name of the output."
        },
        "inputType": {
          "type": "string",
          "description": "Type of function input/output.",
          "enum": [
            "Float1",
            "Float2",
            "Float3",
            "Float4",
            "Texture2D",
            "TextureCube",
            "Bool",
            "MaterialAttributes"
          ]
        },
        "instanceKind": {
          "type": "string",
          "description": "Material function instance kind.",
          "enum": [
            "function",
            "materialLayer",
            "materialLayerBlend"
          ]
        },
        "inputs": {
          "type": "array",
          "description": "Custom expression input definitions.",
          "items": {
            "type": "object"
          }
        },
        "additionalOutputs": {
          "type": "array",
          "description": "Custom expression additional output definitions.",
          "items": {
            "type": "object"
          }
        },
        "additionalDefines": {
          "type": "array",
          "description": "Custom expression additional define definitions.",
          "items": {
            "type": "object"
          }
        },
        "includeFilePaths": {
          "type": "array",
          "description": "Custom expression include file paths.",
          "items": {
            "type": "string"
          }
        },
        "layerName": {
          "type": "string",
          "description": "Name of the layer."
        },
        "blendType": {
          "type": "string",
          "description": "Landscape layer blend type.",
          "enum": [
            "LB_WeightBlend",
            "LB_AlphaBlend",
            "LB_HeightBlend"
          ]
        },
        "layers": {
          "type": "array",
          "description": "Array of layer configurations for layer blend.",
          "items": {
            "type": "object"
          }
        },
        "expressionGuid": {
          "type": "string",
          "description": "Material expression GUID for node selection (get_custom_expression)."
        },
        "expressionIndex": {
          "type": "number",
          "description": "Material expression index for node selection (get_custom_expression)."
        },
        "expressionName": {
          "type": "string",
          "description": "Material expression object name for node selection (get_custom_expression)."
        },
        "save": {
          "type": "boolean",
          "description": "Save the asset(s) after the operation."
        }
      },
      "required": [
        "subAction"
      ]
    }
  },
  {
    "name": "manage_material_diagnostics",
    "description": "Inspect, validate, repair, compile, save, and reload material-family assets.",
    "category": "authoring",
    "inputSchema": {
      "type": "object",
      "properties": {
        "subAction": {
          "type": "string",
          "description": "Material diagnostics sub-action to perform.",
          "enum": [
            "dump_raw_graph",
            "dump_raw_expression",
            "dump_pins",
            "dump_parameter_namespace",
            "validate_material_graph",
            "repair_function_call_pins",
            "repair_custom_expression_outputs",
            "repair_missing_expression_guids",
            "remove_null_expressions",
            "compile_material_diagnostics",
            "verify_save_reload"
          ]
        },
        "assetPath": {
          "type": "string",
          "description": "Material, material function, material instance, or material function instance asset path."
        },
        "expression": {
          "type": "object",
          "description": "ExpressionTarget object or string reference.",
          "additionalProperties": true
        },
        "maxDepth": {
          "type": "integer",
          "description": "Maximum reflected dump depth. Default: 3."
        },
        "verbosity": {
          "type": "string",
          "description": "Raw dump verbosity. Default: normal.",
          "enum": [
            "summary",
            "normal",
            "full"
          ]
        },
        "classAllowList": {
          "type": "array",
          "description": "Allowed expression class names. Empty means all classes.",
          "items": {
            "type": "string"
          }
        },
        "propertyAllowList": {
          "type": "array",
          "description": "Allowed reflected property names. Empty means all properties.",
          "items": {
            "type": "string"
          }
        },
        "includeMainMaterialPins": {
          "type": "boolean",
          "description": "Include owner-level material pins for UMaterial assets."
        },
        "includeInherited": {
          "type": "boolean",
          "description": "Include inherited parameter values when available."
        },
        "includeLayered": {
          "type": "boolean",
          "description": "Include layered parameter identities when available."
        },
        "checks": {
          "type": "array",
          "description": "Validation checks to run. Empty means default checks.",
          "items": {
            "type": "string"
          }
        },
        "expressions": {
          "type": "array",
          "description": "ExpressionTarget objects for selective repair.",
          "items": {
            "type": "object"
          }
        },
        "timeoutSeconds": {
          "type": "number",
          "description": "Compile poll timeout in seconds. Default: 10."
        },
        "save": {
          "type": "boolean",
          "description": "Save before repair or reload verification."
        },
        "includeRawSummary": {
          "type": "boolean",
          "description": "Include before/after raw graph summaries for reload verification."
        }
      },
      "required": [
        "subAction",
        "assetPath"
      ]
    }
  },
  {
    "name": "manage_navigation",
    "description": "Configure NavMesh settings, add nav modifiers, create nav links and smart links for pathfinding.",
    "category": "gameplay",
    "inputSchema": {
      "type": "object",
      "properties": {
        "action": {
          "type": "string",
          "description": "Navigation action to perform",
          "enum": [
            "configure_nav_mesh_settings",
            "set_nav_agent_properties",
            "rebuild_navigation",
            "create_nav_modifier_component",
            "set_nav_area_class",
            "configure_nav_area_cost",
            "create_nav_link_proxy",
            "configure_nav_link",
            "set_nav_link_type",
            "create_smart_link",
            "configure_smart_link_behavior",
            "get_navigation_info"
          ]
        },
        "navMeshPath": {
          "type": "string",
          "description": "Path to NavMesh data asset."
        },
        "actorName": {
          "type": "string",
          "description": "Name of the actor."
        },
        "actorPath": {
          "type": "string",
          "description": "Path to actor."
        },
        "blueprintPath": {
          "type": "string",
          "description": "Blueprint asset path."
        },
        "agentRadius": {
          "type": "number",
          "description": "Navigation agent radius (default: 35)."
        },
        "agentHeight": {
          "type": "number",
          "description": "Navigation agent height (default: 144)."
        },
        "agentStepHeight": {
          "type": "number",
          "description": "Maximum step height agent can climb (default: 35)."
        },
        "agentMaxSlope": {
          "type": "number",
          "description": "Maximum slope angle in degrees (default: 44)."
        },
        "cellSize": {
          "type": "number",
          "description": "NavMesh cell size (default: 19)."
        },
        "cellHeight": {
          "type": "number",
          "description": "NavMesh cell height (default: 10)."
        },
        "tileSizeUU": {
          "type": "number",
          "description": "NavMesh tile size in UU (default: 1000)."
        },
        "minRegionArea": {
          "type": "number",
          "description": "Minimum region area to keep."
        },
        "mergeRegionSize": {
          "type": "number",
          "description": "Region merge threshold."
        },
        "maxSimplificationError": {
          "type": "number",
          "description": "Edge simplification error."
        },
        "componentName": {
          "type": "string",
          "description": "Name of the component."
        },
        "areaClass": {
          "type": "string",
          "description": "Navigation area class path."
        },
        "areaClassToReplace": {
          "type": "string",
          "description": "Area class to replace (optional modifier behavior)."
        },
        "failsafeExtent": {
          "type": "object",
          "description": "Failsafe extent for nav modifier when actor has no collision.",
          "properties": {
            "x": {
              "type": "number"
            },
            "y": {
              "type": "number"
            },
            "z": {
              "type": "number"
            }
          }
        },
        "bIncludeAgentHeight": {
          "type": "boolean",
          "description": "Expand lower bounds by agent height."
        },
        "areaCost": {
          "type": "number",
          "description": "Pathfinding cost multiplier for area (1.0 = normal)."
        },
        "fixedAreaEnteringCost": {
          "type": "number",
          "description": "Fixed cost added when entering the area."
        },
        "linkName": {
          "type": "string",
          "description": "Name of the link."
        },
        "startPoint": {
          "type": "object",
          "description": "Start point of navigation link (relative to actor).",
          "properties": {
            "x": {
              "type": "number"
            },
            "y": {
              "type": "number"
            },
            "z": {
              "type": "number"
            }
          }
        },
        "endPoint": {
          "type": "object",
          "description": "End point of navigation link (relative to actor).",
          "properties": {
            "x": {
              "type": "number"
            },
            "y": {
              "type": "number"
            },
            "z": {
              "type": "number"
            }
          }
        },
        "direction": {
          "type": "string",
          "description": "Link traversal direction.",
          "enum": [
            "BothWays",
            "LeftToRight",
            "RightToLeft"
          ]
        },
        "snapRadius": {
          "type": "number",
          "description": "Snap radius for link endpoints (default: 30)."
        },
        "linkEnabled": {
          "type": "boolean",
          "description": "Whether the link is enabled."
        },
        "linkType": {
          "type": "string",
          "description": "Type of navigation link.",
          "enum": [
            "simple",
            "smart"
          ]
        },
        "bSmartLinkIsRelevant": {
          "type": "boolean",
          "description": "Toggle smart link relevancy."
        },
        "enabledAreaClass": {
          "type": "string",
          "description": "Area class when smart link is enabled."
        },
        "disabledAreaClass": {
          "type": "string",
          "description": "Area class when smart link is disabled."
        },
        "broadcastRadius": {
          "type": "number",
          "description": "Radius for state change broadcast."
        },
        "broadcastInterval": {
          "type": "number",
          "description": "Interval for state change broadcast (0 = single)."
        },
        "bCreateBoxObstacle": {
          "type": "boolean",
          "description": "Add box obstacle during nav generation."
        },
        "obstacleOffset": {
          "type": "object",
          "description": "Offset of simple box obstacle.",
          "properties": {
            "x": {
              "type": "number"
            },
            "y": {
              "type": "number"
            },
            "z": {
              "type": "number"
            }
          }
        },
        "obstacleExtent": {
          "type": "object",
          "description": "Extent of simple box obstacle.",
          "properties": {
            "x": {
              "type": "number"
            },
            "y": {
              "type": "number"
            },
            "z": {
              "type": "number"
            }
          }
        },
        "obstacleAreaClass": {
          "type": "string",
          "description": "Area class for box obstacle."
        },
        "location": {
          "type": "object",
          "description": "World location for nav link proxy.",
          "properties": {
            "x": {
              "type": "number"
            },
            "y": {
              "type": "number"
            },
            "z": {
              "type": "number"
            }
          }
        },
        "rotation": {
          "type": "object",
          "description": "Rotation for nav link proxy.",
          "properties": {
            "pitch": {
              "type": "number"
            },
            "yaw": {
              "type": "number"
            },
            "roll": {
              "type": "number"
            }
          }
        },
        "filter": {
          "type": "string",
          "description": "General search filter."
        },
        "save": {
          "type": "boolean",
          "description": "Save the asset(s) after the operation."
        }
      },
      "required": [
        "action"
      ]
    }
  },
  {
    "name": "manage_networking",
    "description": "Configure multiplayer: property replication, RPCs (Server/Client/Multicast), authority, relevancy, and network prediction.",
    "category": "utility",
    "inputSchema": {
      "type": "object",
      "properties": {
        "action": {
          "type": "string",
          "description": "Networking action to perform",
          "enum": [
            "set_property_replicated",
            "set_replication_condition",
            "configure_net_update_frequency",
            "configure_net_priority",
            "set_net_dormancy",
            "configure_replication_graph",
            "create_rpc_function",
            "configure_rpc_validation",
            "set_rpc_reliability",
            "set_owner",
            "set_autonomous_proxy",
            "check_has_authority",
            "check_is_locally_controlled",
            "configure_net_cull_distance",
            "set_always_relevant",
            "set_only_relevant_to_owner",
            "configure_net_serialization",
            "set_replicated_using",
            "configure_push_model",
            "configure_client_prediction",
            "configure_server_correction",
            "add_network_prediction_data",
            "configure_movement_prediction",
            "configure_net_driver",
            "set_net_role",
            "configure_replicated_movement",
            "get_networking_info"
          ]
        },
        "blueprintPath": {
          "type": "string",
          "description": "Blueprint asset path."
        },
        "actorName": {
          "type": "string",
          "description": "Name of the actor."
        },
        "propertyName": {
          "type": "string",
          "description": "Name of the property."
        },
        "replicated": {
          "type": "boolean",
          "description": "Whether property should be replicated."
        },
        "condition": {
          "type": "string",
          "description": "Replication condition.",
          "enum": [
            "COND_None",
            "COND_InitialOnly",
            "COND_OwnerOnly",
            "COND_SkipOwner",
            "COND_SimulatedOnly",
            "COND_AutonomousOnly",
            "COND_SimulatedOrPhysics",
            "COND_InitialOrOwner",
            "COND_Custom",
            "COND_ReplayOrOwner",
            "COND_ReplayOnly",
            "COND_SimulatedOnlyNoReplay",
            "COND_SimulatedOrPhysicsNoReplay",
            "COND_SkipReplay",
            "COND_Never"
          ]
        },
        "repNotifyFunc": {
          "type": "string",
          "description": "RepNotify function name."
        },
        "netUpdateFrequency": {
          "type": "number",
          "description": "How often actor replicates (Hz, default 100)."
        },
        "minNetUpdateFrequency": {
          "type": "number",
          "description": "Minimum update frequency when idle (Hz, default 2)."
        },
        "netPriority": {
          "type": "number",
          "description": "Network priority for bandwidth (default 1.0)."
        },
        "dormancy": {
          "type": "string",
          "description": "Net dormancy mode.",
          "enum": [
            "DORM_Never",
            "DORM_Awake",
            "DORM_DormantAll",
            "DORM_DormantPartial",
            "DORM_Initial"
          ]
        },
        "nodeClass": {
          "type": "string",
          "description": "Node class path."
        },
        "spatialBias": {
          "type": "number",
          "description": "Spatial bias for replication graph."
        },
        "defaultSettingsClass": {
          "type": "string",
          "description": "Default replication settings class."
        },
        "functionName": {
          "type": "string",
          "description": "Name of the function."
        },
        "rpcType": {
          "type": "string",
          "description": "Type of RPC.",
          "enum": [
            "Server",
            "Client",
            "NetMulticast"
          ]
        },
        "reliable": {
          "type": "boolean",
          "description": "Whether the operation is reliable."
        },
        "parameters": {
          "type": "array",
          "description": "RPC function parameters.",
          "items": {
            "type": "object",
            "properties": {
              "name": {
                "type": "string"
              },
              "type": {
                "type": "string"
              }
            }
          }
        },
        "returnType": {
          "type": "string",
          "description": "RPC return type (usually void)."
        },
        "validationFunctionName": {
          "type": "string",
          "description": "Name of validation function."
        },
        "withValidation": {
          "type": "boolean",
          "description": "Enable RPC validation."
        },
        "ownerActorName": {
          "type": "string",
          "description": "Name of owner actor (null to clear)."
        },
        "isAutonomousProxy": {
          "type": "boolean",
          "description": "Configure as autonomous proxy."
        },
        "netCullDistanceSquared": {
          "type": "number",
          "description": "Network cull distance squared."
        },
        "useOwnerNetRelevancy": {
          "type": "boolean",
          "description": "Use owner relevancy."
        },
        "alwaysRelevant": {
          "type": "boolean",
          "description": "Always relevant to all clients."
        },
        "onlyRelevantToOwner": {
          "type": "boolean",
          "description": "Only relevant to owner."
        },
        "structName": {
          "type": "string",
          "description": "Name of struct for custom serialization."
        },
        "useNetSerialize": {
          "type": "boolean",
          "description": "Use custom NetSerialize."
        },
        "usePushModel": {
          "type": "boolean",
          "description": "Use push-model replication."
        },
        "propertyNames": {
          "type": "array",
          "description": "Properties for push model.",
          "items": {
            "type": "string"
          }
        },
        "enablePrediction": {
          "type": "boolean",
          "description": "Enable client-side prediction."
        },
        "predictionKey": {
          "type": "string",
          "description": "Prediction key identifier."
        },
        "correctionThreshold": {
          "type": "number",
          "description": "Server correction threshold."
        },
        "smoothingRate": {
          "type": "number",
          "description": "Smoothing rate for corrections."
        },
        "dataType": {
          "type": "string",
          "description": "Network prediction data type.",
          "enum": [
            "InputCmd",
            "SyncState",
            "AuxState"
          ]
        },
        "properties": {
          "type": "array",
          "description": "Predicted properties.",
          "items": {
            "type": "object",
            "properties": {
              "name": {
                "type": "string"
              },
              "type": {
                "type": "string"
              }
            }
          }
        },
        "networkSmoothingMode": {
          "type": "string",
          "description": "Movement smoothing mode.",
          "enum": [
            "Disabled",
            "Linear",
            "Exponential"
          ]
        },
        "networkMaxSmoothUpdateDistance": {
          "type": "number",
          "description": "Max smooth update distance."
        },
        "networkNoSmoothUpdateDistance": {
          "type": "number",
          "description": "No smooth update distance."
        },
        "maxClientRate": {
          "type": "number",
          "description": "Max client rate."
        },
        "maxInternetClientRate": {
          "type": "number",
          "description": "Max internet client rate."
        },
        "netServerMaxTickRate": {
          "type": "number",
          "description": "Server max tick rate."
        },
        "role": {
          "type": "string",
          "description": "Net role.",
          "enum": [
            "ROLE_None",
            "ROLE_SimulatedProxy",
            "ROLE_AutonomousProxy",
            "ROLE_Authority"
          ]
        },
        "replicateMovement": {
          "type": "boolean",
          "description": "Replicate movement."
        },
        "replicatedMovementMode": {
          "type": "string",
          "description": "Replicated movement mode.",
          "enum": [
            "Default",
            "SkipPhysics",
            "FullMovement"
          ]
        },
        "locationQuantizationLevel": {
          "type": "string",
          "description": "Location quantization level.",
          "enum": [
            "RoundWholeNumber",
            "RoundOneDecimal",
            "RoundTwoDecimals"
          ]
        },
        "spatiallyLoaded": {
          "type": "boolean",
          "description": "Spatially loaded for replication graph."
        },
        "netLoadOnClient": {
          "type": "boolean",
          "description": "Net load on client for replication graph."
        },
        "replicationPolicy": {
          "type": "string",
          "description": "Replication policy for replication graph."
        },
        "customSerialization": {
          "type": "boolean",
          "description": "Use custom serialization."
        },
        "predictionThreshold": {
          "type": "number",
          "description": "Prediction threshold for client prediction."
        }
      },
      "required": [
        "action"
      ]
    }
  },
  {
    "name": "manage_performance",
    "description": "Run profiling/benchmarks, configure scalability, LOD, Nanite, and optimization settings.",
    "category": "utility",
    "inputSchema": {
      "type": "object",
      "properties": {
        "action": {
          "type": "string",
          "description": "Action",
          "enum": [
            "start_profiling",
            "stop_profiling",
            "run_benchmark",
            "show_fps",
            "show_stats",
            "generate_memory_report",
            "set_scalability",
            "set_resolution_scale",
            "set_vsync",
            "set_frame_rate_limit",
            "enable_gpu_timing",
            "configure_texture_streaming",
            "configure_lod",
            "apply_baseline_settings",
            "optimize_draw_calls",
            "merge_actors",
            "configure_occlusion_culling",
            "optimize_shaders",
            "configure_nanite",
            "configure_world_partition"
          ]
        },
        "type": {
          "type": "string",
          "enum": [
            "CPU",
            "GPU",
            "Memory",
            "RenderThread",
            "GameThread",
            "All"
          ]
        },
        "duration": {
          "type": "number"
        },
        "outputPath": {
          "type": "string",
          "description": "Output file or directory path."
        },
        "detailed": {
          "type": "boolean"
        },
        "category": {
          "type": "string"
        },
        "level": {
          "type": "number"
        },
        "scale": {
          "type": "number"
        },
        "enabled": {
          "type": "boolean",
          "description": "Whether the item/feature is enabled."
        },
        "maxFPS": {
          "type": "number"
        },
        "verbose": {
          "type": "boolean"
        },
        "poolSize": {
          "type": "number"
        },
        "boostPlayerLocation": {
          "type": "boolean"
        },
        "forceLOD": {
          "type": "number"
        },
        "lodBias": {
          "type": "number"
        },
        "distanceScale": {
          "type": "number"
        },
        "skeletalBias": {
          "type": "number"
        },
        "hzb": {
          "type": "boolean"
        },
        "enableInstancing": {
          "type": "boolean"
        },
        "enableBatching": {
          "type": "boolean"
        },
        "mergeActors": {
          "type": "boolean"
        },
        "actors": {
          "type": "array",
          "items": {
            "type": "string"
          }
        },
        "freezeRendering": {
          "type": "boolean"
        },
        "compileOnDemand": {
          "type": "boolean"
        },
        "cacheShaders": {
          "type": "boolean"
        },
        "reducePermutations": {
          "type": "boolean"
        },
        "maxPixelsPerEdge": {
          "type": "number"
        },
        "streamingPoolSize": {
          "type": "number"
        },
        "streamingDistance": {
          "type": "number"
        },
        "cellSize": {
          "type": "number"
        }
      },
      "required": [
        "action"
      ]
    }
  },
  {
    "name": "manage_pipeline",
    "description": "Build automation and pipeline control. Actions: run_ubt (compile targets), list_categories (show tool categories), get_status (bridge status). Routes to system_control internally.",
    "category": "core",
    "inputSchema": {
      "type": "object",
      "properties": {
        "action": {
          "type": "string",
          "description": "run_ubt: compile with UnrealBuildTool. list_categories: show available tool categories. get_status: get bridge status.",
          "enum": [
            "run_ubt",
            "list_categories",
            "get_status"
          ]
        },
        "target": {
          "type": "string",
          "description": "Build target name (e.g., MyProjectEditor)"
        },
        "platform": {
          "type": "string",
          "description": "Target platform (Win64, Linux, Mac)"
        },
        "configuration": {
          "type": "string",
          "description": "Build configuration (Development, Shipping, Debug)"
        },
        "arguments": {
          "type": "string",
          "description": "Additional UBT arguments"
        }
      },
      "required": [
        "action"
      ]
    }
  },
  {
    "name": "manage_sequence",
    "description": "Edit Level Sequences: add tracks, bind actors, set keyframes, control playback, and record camera.",
    "category": "utility",
    "inputSchema": {
      "type": "object",
      "properties": {
        "action": {
          "type": "string",
          "description": "Action",
          "enum": [
            "create",
            "open",
            "add_camera",
            "add_actor",
            "add_actors",
            "remove_actors",
            "get_bindings",
            "play",
            "pause",
            "stop",
            "set_playback_speed",
            "add_keyframe",
            "get_properties",
            "set_properties",
            "duplicate",
            "rename",
            "delete",
            "list",
            "get_metadata",
            "set_metadata",
            "add_spawnable_from_class",
            "add_track",
            "add_section",
            "set_display_rate",
            "set_tick_resolution",
            "set_work_range",
            "set_view_range",
            "set_track_muted",
            "set_track_solo",
            "set_track_locked",
            "list_tracks",
            "remove_track",
            "list_track_types"
          ]
        },
        "name": {
          "type": "string",
          "description": "Name identifier."
        },
        "path": {
          "type": "string",
          "description": "Asset path (e.g., /Game/Path/Asset)."
        },
        "actorName": {
          "type": "string",
          "description": "Name of the actor."
        },
        "actorNames": {
          "type": "array",
          "items": {
            "type": "string"
          }
        },
        "frame": {
          "type": "number"
        },
        "value": {
          "type": "object",
          "additionalProperties": true
        },
        "property": {
          "type": "string",
          "description": "Name of the property."
        },
        "destinationPath": {
          "type": "string",
          "description": "Destination path for move/copy."
        },
        "newName": {
          "type": "string",
          "description": "New name for renaming."
        },
        "overwrite": {
          "type": "boolean",
          "description": "Overwrite if the asset/file already exists."
        },
        "speed": {
          "type": "number"
        },
        "startTime": {
          "type": "number"
        },
        "loopMode": {
          "type": "string"
        },
        "className": {
          "type": "string"
        },
        "spawnable": {
          "type": "boolean"
        },
        "trackType": {
          "type": "string"
        },
        "trackName": {
          "type": "string"
        },
        "muted": {
          "type": "boolean"
        },
        "solo": {
          "type": "boolean"
        },
        "locked": {
          "type": "boolean"
        },
        "assetPath": {
          "type": "string",
          "description": "Asset path (e.g., /Game/Path/Asset)."
        },
        "startFrame": {
          "type": "number"
        },
        "endFrame": {
          "type": "number"
        },
        "frameRate": {
          "type": "string"
        },
        "resolution": {
          "type": "string"
        },
        "start": {
          "type": "number"
        },
        "end": {
          "type": "number"
        },
        "lengthInFrames": {
          "type": "number"
        },
        "playbackStart": {
          "type": "number"
        },
        "playbackEnd": {
          "type": "number"
        },
        "metadata": {
          "type": "object",
          "additionalProperties": true
        }
      },
      "required": [
        "action"
      ]
    }
  },
  {
    "name": "manage_sessions",
    "description": "Configure local multiplayer: split-screen layouts, LAN hosting/joining, voice chat channels, and push-to-talk.\n\nRequired parameters by action:\n- configure_local_session_settings: At least one setting param required\n- configure_session_interface: interfaceType\n- configure_split_screen: enabled or splitScreenType\n- set_split_screen_type: splitScreenType\n- add_local_player: controllerId (requires PIE)\n- remove_local_player: playerIndex (requires PIE)\n- configure_lan_play: enabled, serverPort, or serverPassword\n- host_lan_server: mapName (requires PIE)\n- join_lan_server: serverAddress (requires PIE)\n- enable_voice_chat: voiceEnabled\n- configure_voice_settings: voiceSettings\n- set_voice_channel: channelName\n- mute_player: playerName or targetPlayerId\n- set_voice_attenuation: attenuationRadius\n- configure_push_to_talk: pushToTalkEnabled\n- get_sessions_info: No additional params required",
    "category": "utility",
    "inputSchema": {
      "type": "object",
      "properties": {
        "action": {
          "type": "string",
          "description": "Sessions action to perform.",
          "enum": [
            "configure_local_session_settings",
            "configure_session_interface",
            "configure_split_screen",
            "set_split_screen_type",
            "add_local_player",
            "remove_local_player",
            "configure_lan_play",
            "host_lan_server",
            "join_lan_server",
            "enable_voice_chat",
            "configure_voice_settings",
            "set_voice_channel",
            "mute_player",
            "set_voice_attenuation",
            "configure_push_to_talk",
            "get_sessions_info"
          ]
        },
        "sessionName": {
          "type": "string",
          "description": "Name of the session."
        },
        "sessionId": {
          "type": "string",
          "description": "Session ID."
        },
        "maxPlayers": {
          "type": "number"
        },
        "bIsLANMatch": {
          "type": "boolean",
          "description": "Whether this is a LAN match."
        },
        "bAllowJoinInProgress": {
          "type": "boolean",
          "description": "Allow joining games in progress."
        },
        "bAllowInvites": {
          "type": "boolean",
          "description": "Allow player invites."
        },
        "bUsesPresence": {
          "type": "boolean",
          "description": "Use presence for session discovery."
        },
        "bUseLobbiesIfAvailable": {
          "type": "boolean",
          "description": "Use lobby system if available."
        },
        "bShouldAdvertise": {
          "type": "boolean",
          "description": "Advertise session publicly."
        },
        "interfaceType": {
          "type": "string",
          "description": "Type of session interface to use. REQUIRED for configure_session_interface.",
          "enum": [
            "Default",
            "LAN",
            "Null"
          ]
        },
        "enabled": {
          "type": "boolean",
          "description": "Whether the item/feature is enabled."
        },
        "splitScreenType": {
          "type": "string",
          "description": "Split-screen layout type. REQUIRED for set_split_screen_type.",
          "enum": [
            "None",
            "TwoPlayer_Horizontal",
            "TwoPlayer_Vertical",
            "ThreePlayer_FavorTop",
            "ThreePlayer_FavorBottom",
            "FourPlayer_Grid"
          ]
        },
        "playerIndex": {
          "type": "number",
          "description": "Local player index. REQUIRED for remove_local_player."
        },
        "controllerId": {
          "type": "number",
          "description": "Controller ID for player input. REQUIRED for add_local_player."
        },
        "serverAddress": {
          "type": "string",
          "description": "Server IP address. REQUIRED for join_lan_server."
        },
        "serverPort": {
          "type": "number"
        },
        "serverPassword": {
          "type": "string",
          "description": "Server password for protected games."
        },
        "serverName": {
          "type": "string",
          "description": "Display name for the server."
        },
        "mapName": {
          "type": "string",
          "description": "Map to load for hosting. REQUIRED for host_lan_server."
        },
        "travelOptions": {
          "type": "string",
          "description": "Travel URL options string."
        },
        "voiceEnabled": {
          "type": "boolean",
          "description": "Enable/disable voice chat. REQUIRED for enable_voice_chat."
        },
        "voiceSettings": {
          "type": "object",
          "description": "Voice processing settings. REQUIRED for configure_voice_settings.",
          "properties": {
            "volume": {
              "type": "number",
              "description": "Voice volume (0.0 - 1.0)."
            },
            "noiseGateThreshold": {
              "type": "number",
              "description": "Noise gate threshold."
            },
            "noiseSuppression": {
              "type": "boolean",
              "description": "Enable noise suppression."
            },
            "echoCancellation": {
              "type": "boolean",
              "description": "Enable echo cancellation."
            },
            "sampleRate": {
              "type": "number",
              "description": "Audio sample rate in Hz."
            }
          }
        },
        "channelName": {
          "type": "string",
          "description": "Voice channel name. REQUIRED for set_voice_channel."
        },
        "channelType": {
          "type": "string",
          "description": "Voice channel type.",
          "enum": [
            "Team",
            "Global",
            "Proximity",
            "Party"
          ]
        },
        "playerName": {
          "type": "string",
          "description": "Player name for voice operations. REQUIRED for mute_player (or targetPlayerId)."
        },
        "targetPlayerId": {
          "type": "string",
          "description": "Target player ID. REQUIRED for mute_player (or playerName)."
        },
        "muted": {
          "type": "boolean",
          "description": "Whether the item is muted."
        },
        "attenuationRadius": {
          "type": "number",
          "description": "Radius for voice attenuation (Proximity chat). REQUIRED for set_voice_attenuation."
        },
        "attenuationFalloff": {
          "type": "number",
          "description": "Falloff rate for voice attenuation."
        },
        "pushToTalkEnabled": {
          "type": "boolean",
          "description": "Enable push-to-talk mode. REQUIRED for configure_push_to_talk."
        },
        "pushToTalkKey": {
          "type": "string",
          "description": "Key binding for push-to-talk."
        }
      },
      "required": [
        "action"
      ]
    }
  },
  {
    "name": "manage_skeleton",
    "description": "Edit skeletal meshes: add sockets, configure physics assets, set skin weights, and create morph targets.",
    "category": "authoring",
    "inputSchema": {
      "type": "object",
      "properties": {
        "action": {
          "type": "string",
          "description": "Skeleton action to perform",
          "enum": [
            "create_skeleton",
            "add_bone",
            "remove_bone",
            "rename_bone",
            "set_bone_transform",
            "set_bone_parent",
            "create_virtual_bone",
            "create_socket",
            "configure_socket",
            "auto_skin_weights",
            "set_vertex_weights",
            "normalize_weights",
            "prune_weights",
            "copy_weights",
            "mirror_weights",
            "create_physics_asset",
            "add_physics_body",
            "configure_physics_body",
            "add_physics_constraint",
            "configure_constraint_limits",
            "bind_cloth_to_skeletal_mesh",
            "assign_cloth_asset_to_mesh",
            "create_morph_target",
            "set_morph_target_deltas",
            "import_morph_targets",
            "get_skeleton_info",
            "list_bones",
            "list_sockets",
            "list_physics_bodies"
          ]
        },
        "skeletonPath": {
          "type": "string",
          "description": "Skeleton asset path."
        },
        "skeletalMeshPath": {
          "type": "string",
          "description": "Skeletal mesh path."
        },
        "physicsAssetPath": {
          "type": "string",
          "description": "Path to physics asset."
        },
        "morphTargetPath": {
          "type": "string",
          "description": "Path to morph target or FBX file for import."
        },
        "clothAssetPath": {
          "type": "string",
          "description": "Path to cloth asset."
        },
        "outputPath": {
          "type": "string",
          "description": "Output file or directory path."
        },
        "boneName": {
          "type": "string",
          "description": "Name of the bone."
        },
        "newBoneName": {
          "type": "string",
          "description": "New name for renaming."
        },
        "parentBoneName": {
          "type": "string",
          "description": "Parent bone name."
        },
        "sourceBoneName": {
          "type": "string",
          "description": "Source bone name."
        },
        "targetBoneName": {
          "type": "string",
          "description": "Target bone name."
        },
        "boneIndex": {
          "type": "number",
          "description": "Bone index for operations."
        },
        "location": {
          "type": "object",
          "description": "3D location (x, y, z).",
          "properties": {
            "x": {
              "type": "number"
            },
            "y": {
              "type": "number"
            },
            "z": {
              "type": "number"
            }
          }
        },
        "rotation": {
          "type": "object",
          "description": "3D rotation (pitch, yaw, roll).",
          "properties": {
            "pitch": {
              "type": "number"
            },
            "yaw": {
              "type": "number"
            },
            "roll": {
              "type": "number"
            }
          }
        },
        "scale": {
          "type": "object",
          "description": "3D scale (x, y, z).",
          "properties": {
            "x": {
              "type": "number"
            },
            "y": {
              "type": "number"
            },
            "z": {
              "type": "number"
            }
          }
        },
        "socketName": {
          "type": "string",
          "description": "Name of the socket."
        },
        "attachBoneName": {
          "type": "string",
          "description": "Bone to attach to."
        },
        "relativeLocation": {
          "type": "object",
          "description": "3D location (x, y, z).",
          "properties": {
            "x": {
              "type": "number"
            },
            "y": {
              "type": "number"
            },
            "z": {
              "type": "number"
            }
          }
        },
        "relativeRotation": {
          "type": "object",
          "description": "3D rotation (pitch, yaw, roll).",
          "properties": {
            "pitch": {
              "type": "number"
            },
            "yaw": {
              "type": "number"
            },
            "roll": {
              "type": "number"
            }
          }
        },
        "relativeScale": {
          "type": "object",
          "description": "3D scale (x, y, z).",
          "properties": {
            "x": {
              "type": "number"
            },
            "y": {
              "type": "number"
            },
            "z": {
              "type": "number"
            }
          }
        },
        "vertexIndex": {
          "type": "number",
          "description": "Vertex index for weight operations."
        },
        "vertexIndices": {
          "type": "array",
          "description": "Array of vertex indices.",
          "items": {
            "type": "number"
          }
        },
        "weights": {
          "type": "array",
          "description": "Array of {boneIndex, weight} pairs.",
          "items": {
            "type": "object"
          }
        },
        "threshold": {
          "type": "number",
          "description": "Weight threshold for pruning (0-1)."
        },
        "mirrorAxis": {
          "type": "string",
          "description": "Axis for weight mirroring.",
          "enum": [
            "X",
            "Y",
            "Z"
          ]
        },
        "mirrorTable": {
          "type": "object",
          "description": "Bone name mapping for mirroring.",
          "additionalProperties": true
        },
        "bodyType": {
          "type": "string",
          "description": "Physics body shape type.",
          "enum": [
            "Capsule",
            "Sphere",
            "Box",
            "Convex",
            "Sphyl"
          ]
        },
        "bodyName": {
          "type": "string",
          "description": "Physics body name."
        },
        "mass": {
          "type": "number",
          "description": "Mass value."
        },
        "linearDamping": {
          "type": "number",
          "description": "Linear damping factor."
        },
        "angularDamping": {
          "type": "number",
          "description": "Angular damping factor."
        },
        "collisionEnabled": {
          "type": "boolean",
          "description": "Enable collision for this body."
        },
        "simulatePhysics": {
          "type": "boolean",
          "description": "Enable physics simulation."
        },
        "constraintName": {
          "type": "string",
          "description": "Constraint name."
        },
        "bodyA": {
          "type": "string",
          "description": "First physics body."
        },
        "bodyB": {
          "type": "string",
          "description": "Second physics body."
        },
        "limits": {
          "type": "object",
          "description": "Constraint angular limits.",
          "properties": {
            "swing1LimitAngle": {
              "type": "number",
              "description": "Swing 1 limit in degrees."
            },
            "swing2LimitAngle": {
              "type": "number",
              "description": "Swing 2 limit in degrees."
            },
            "twistLimitAngle": {
              "type": "number",
              "description": "Twist limit in degrees."
            },
            "swing1Motion": {
              "type": "string",
              "enum": [
                "Free",
                "Limited",
                "Locked"
              ]
            },
            "swing2Motion": {
              "type": "string",
              "enum": [
                "Free",
                "Limited",
                "Locked"
              ]
            },
            "twistMotion": {
              "type": "string",
              "enum": [
                "Free",
                "Limited",
                "Locked"
              ]
            }
          }
        },
        "morphTargetName": {
          "type": "string",
          "description": "Morph target name."
        },
        "deltas": {
          "type": "array",
          "description": "Array of {vertexIndex, delta} for morph target.",
          "items": {
            "type": "object"
          }
        },
        "paintValue": {
          "type": "number",
          "description": "Cloth weight paint value (0-1)."
        },
        "save": {
          "type": "boolean",
          "description": "Save the asset(s) after the operation."
        },
        "overwrite": {
          "type": "boolean",
          "description": "Overwrite if the asset/file already exists."
        }
      },
      "required": [
        "action"
      ]
    }
  },
  {
    "name": "manage_splines",
    "description": "Create spline actors, add/modify points, attach meshes along splines, and query spline data.",
    "category": "world",
    "inputSchema": {
      "type": "object",
      "properties": {
        "action": {
          "type": "string",
          "description": "Spline action to perform",
          "enum": [
            "create_spline_actor",
            "add_spline_point",
            "remove_spline_point",
            "set_spline_point_position",
            "set_spline_point_tangents",
            "set_spline_point_rotation",
            "set_spline_point_scale",
            "set_spline_type",
            "create_spline_mesh_component",
            "set_spline_mesh_asset",
            "configure_spline_mesh_axis",
            "set_spline_mesh_material",
            "scatter_meshes_along_spline",
            "configure_mesh_spacing",
            "configure_mesh_randomization",
            "create_road_spline",
            "create_river_spline",
            "create_fence_spline",
            "create_wall_spline",
            "create_cable_spline",
            "create_pipe_spline",
            "get_splines_info"
          ]
        },
        "actorName": {
          "type": "string",
          "description": "Name of the actor."
        },
        "actorPath": {
          "type": "string",
          "description": "Path to actor."
        },
        "splineName": {
          "type": "string",
          "description": "Name of spline component."
        },
        "componentName": {
          "type": "string",
          "description": "Name of the component."
        },
        "blueprintPath": {
          "type": "string",
          "description": "Blueprint asset path."
        },
        "location": {
          "type": "object",
          "description": "Location for spline actor.",
          "properties": {
            "x": {
              "type": "number"
            },
            "y": {
              "type": "number"
            },
            "z": {
              "type": "number"
            }
          }
        },
        "rotation": {
          "type": "object",
          "description": "Rotation for spline actor.",
          "properties": {
            "pitch": {
              "type": "number"
            },
            "yaw": {
              "type": "number"
            },
            "roll": {
              "type": "number"
            }
          }
        },
        "scale": {
          "type": "object",
          "description": "Scale for spline actor.",
          "properties": {
            "x": {
              "type": "number"
            },
            "y": {
              "type": "number"
            },
            "z": {
              "type": "number"
            }
          }
        },
        "pointIndex": {
          "type": "number",
          "description": "Index of spline point to modify."
        },
        "position": {
          "type": "object",
          "description": "Position for spline point.",
          "properties": {
            "x": {
              "type": "number"
            },
            "y": {
              "type": "number"
            },
            "z": {
              "type": "number"
            }
          }
        },
        "arriveTangent": {
          "type": "object",
          "description": "Arrive tangent for spline point (incoming direction).",
          "properties": {
            "x": {
              "type": "number"
            },
            "y": {
              "type": "number"
            },
            "z": {
              "type": "number"
            }
          }
        },
        "leaveTangent": {
          "type": "object",
          "description": "Leave tangent for spline point (outgoing direction).",
          "properties": {
            "x": {
              "type": "number"
            },
            "y": {
              "type": "number"
            },
            "z": {
              "type": "number"
            }
          }
        },
        "tangent": {
          "type": "object",
          "description": "Unified tangent (sets both arrive and leave).",
          "properties": {
            "x": {
              "type": "number"
            },
            "y": {
              "type": "number"
            },
            "z": {
              "type": "number"
            }
          }
        },
        "pointRotation": {
          "type": "object",
          "description": "Rotation at spline point.",
          "properties": {
            "pitch": {
              "type": "number"
            },
            "yaw": {
              "type": "number"
            },
            "roll": {
              "type": "number"
            }
          }
        },
        "pointScale": {
          "type": "object",
          "description": "Scale at spline point.",
          "properties": {
            "x": {
              "type": "number"
            },
            "y": {
              "type": "number"
            },
            "z": {
              "type": "number"
            }
          }
        },
        "coordinateSpace": {
          "type": "string",
          "description": "Coordinate space for position/tangent values (default: Local).",
          "enum": [
            "Local",
            "World"
          ]
        },
        "splineType": {
          "type": "string",
          "description": "Type of spline interpolation.",
          "enum": [
            "Linear",
            "Curve",
            "Constant",
            "CurveClamped",
            "CurveCustomTangent"
          ]
        },
        "bClosedLoop": {
          "type": "boolean",
          "description": "Whether spline forms a closed loop."
        },
        "bUpdateSpline": {
          "type": "boolean",
          "description": "Update spline after modification (default: true)."
        },
        "meshPath": {
          "type": "string",
          "description": "Mesh asset path."
        },
        "materialPath": {
          "type": "string",
          "description": "Material asset path."
        },
        "forwardAxis": {
          "type": "string",
          "description": "Forward axis for spline mesh deformation.",
          "enum": [
            "X",
            "Y",
            "Z"
          ]
        },
        "startPos": {
          "type": "object",
          "description": "Start position for spline mesh segment.",
          "properties": {
            "x": {
              "type": "number"
            },
            "y": {
              "type": "number"
            },
            "z": {
              "type": "number"
            }
          }
        },
        "startTangent": {
          "type": "object",
          "description": "Start tangent for spline mesh segment.",
          "properties": {
            "x": {
              "type": "number"
            },
            "y": {
              "type": "number"
            },
            "z": {
              "type": "number"
            }
          }
        },
        "endPos": {
          "type": "object",
          "description": "End position for spline mesh segment.",
          "properties": {
            "x": {
              "type": "number"
            },
            "y": {
              "type": "number"
            },
            "z": {
              "type": "number"
            }
          }
        },
        "endTangent": {
          "type": "object",
          "description": "End tangent for spline mesh segment.",
          "properties": {
            "x": {
              "type": "number"
            },
            "y": {
              "type": "number"
            },
            "z": {
              "type": "number"
            }
          }
        },
        "startScale": {
          "type": "object",
          "description": "X/Y scale at spline mesh start.",
          "properties": {
            "x": {
              "type": "number"
            },
            "y": {
              "type": "number"
            }
          }
        },
        "endScale": {
          "type": "object",
          "description": "X/Y scale at spline mesh end.",
          "properties": {
            "x": {
              "type": "number"
            },
            "y": {
              "type": "number"
            }
          }
        },
        "startRoll": {
          "type": "number",
          "description": "Roll angle at spline mesh start (radians)."
        },
        "endRoll": {
          "type": "number",
          "description": "Roll angle at spline mesh end (radians)."
        },
        "bSmoothInterpRollScale": {
          "type": "boolean",
          "description": "Use smooth interpolation for roll/scale."
        },
        "spacing": {
          "type": "number",
          "description": "Distance between scattered meshes."
        },
        "startOffset": {
          "type": "number",
          "description": "Offset from spline start for first mesh."
        },
        "endOffset": {
          "type": "number",
          "description": "Offset from spline end for last mesh."
        },
        "bAlignToSpline": {
          "type": "boolean",
          "description": "Align scattered meshes to spline direction."
        },
        "bRandomizeRotation": {
          "type": "boolean",
          "description": "Apply random rotation to scattered meshes."
        },
        "rotationRandomRange": {
          "type": "object",
          "description": "Random rotation range (degrees).",
          "properties": {
            "pitch": {
              "type": "number"
            },
            "yaw": {
              "type": "number"
            },
            "roll": {
              "type": "number"
            }
          }
        },
        "bRandomizeScale": {
          "type": "boolean",
          "description": "Apply random scale to scattered meshes."
        },
        "scaleMin": {
          "type": "number",
          "description": "Minimum random scale multiplier."
        },
        "scaleMax": {
          "type": "number",
          "description": "Maximum random scale multiplier."
        },
        "randomSeed": {
          "type": "number",
          "description": "Seed for randomization (for reproducible results)."
        },
        "templateType": {
          "type": "string",
          "description": "Type of spline template to create.",
          "enum": [
            "road",
            "river",
            "fence",
            "wall",
            "cable",
            "pipe"
          ]
        },
        "width": {
          "type": "number",
          "description": "Width value."
        },
        "segmentLength": {
          "type": "number",
          "description": "Length of mesh segments for deformation."
        },
        "postSpacing": {
          "type": "number",
          "description": "Spacing between fence posts."
        },
        "railHeight": {
          "type": "number",
          "description": "Height of fence rails."
        },
        "pipeRadius": {
          "type": "number",
          "description": "Radius for pipe template."
        },
        "cableSlack": {
          "type": "number",
          "description": "Slack/sag amount for cable template."
        },
        "points": {
          "type": "array",
          "items": {
            "type": "object"
          }
        },
        "filter": {
          "type": "string",
          "description": "General search filter."
        },
        "save": {
          "type": "boolean",
          "description": "Save the asset(s) after the operation."
        }
      },
      "required": [
        "action"
      ]
    }
  },
  {
    "name": "manage_texture",
    "description": "Create procedural textures, process images, bake normal/AO maps, and set compression settings.",
    "category": "authoring",
    "inputSchema": {
      "type": "object",
      "properties": {
        "action": {
          "type": "string",
          "description": "Texture action to perform",
          "enum": [
            "create_noise_texture",
            "create_gradient_texture",
            "create_pattern_texture",
            "create_normal_from_height",
            "create_ao_from_mesh",
            "resize_texture",
            "adjust_levels",
            "adjust_curves",
            "blur",
            "sharpen",
            "invert",
            "desaturate",
            "channel_pack",
            "channel_extract",
            "combine_textures",
            "set_compression_settings",
            "set_texture_group",
            "set_lod_bias",
            "configure_virtual_texture",
            "set_streaming_priority",
            "get_texture_info"
          ]
        },
        "assetPath": {
          "type": "string",
          "description": "Asset path (e.g., /Game/Path/Asset)."
        },
        "name": {
          "type": "string",
          "description": "Name identifier."
        },
        "path": {
          "type": "string",
          "description": "Directory path for asset creation."
        },
        "width": {
          "type": "number",
          "description": "Width value."
        },
        "height": {
          "type": "number",
          "description": "Height value."
        },
        "newWidth": {
          "type": "number",
          "description": "New width for resize operation."
        },
        "newHeight": {
          "type": "number",
          "description": "New height for resize operation."
        },
        "noiseType": {
          "type": "string",
          "description": "Type of noise to generate.",
          "enum": [
            "Perlin",
            "Simplex",
            "Worley",
            "Voronoi"
          ]
        },
        "scale": {
          "type": "number",
          "description": "Noise scale/frequency."
        },
        "octaves": {
          "type": "number",
          "description": "Number of noise octaves for FBM."
        },
        "persistence": {
          "type": "number",
          "description": "Amplitude falloff per octave."
        },
        "lacunarity": {
          "type": "number",
          "description": "Frequency multiplier per octave."
        },
        "seed": {
          "type": "number",
          "description": "Random seed for procedural generation."
        },
        "seamless": {
          "type": "boolean",
          "description": "Generate seamless/tileable texture."
        },
        "gradientType": {
          "type": "string",
          "description": "Type of gradient.",
          "enum": [
            "Linear",
            "Radial",
            "Angular"
          ]
        },
        "startColor": {
          "type": "object",
          "description": "Start color {r, g, b, a}.",
          "additionalProperties": true
        },
        "endColor": {
          "type": "object",
          "description": "End color {r, g, b, a}.",
          "additionalProperties": true
        },
        "angle": {
          "type": "number",
          "description": "Angle in degrees."
        },
        "centerX": {
          "type": "number",
          "description": "Center X position (0-1) for radial gradient."
        },
        "centerY": {
          "type": "number",
          "description": "Center Y position (0-1) for radial gradient."
        },
        "radius": {
          "type": "number",
          "description": "Radius value."
        },
        "colorStops": {
          "type": "array",
          "description": "Array of {position, color} for multi-color gradients.",
          "items": {
            "type": "object"
          }
        },
        "patternType": {
          "type": "string",
          "description": "Type of pattern.",
          "enum": [
            "Checker",
            "Grid",
            "Brick",
            "Tile",
            "Dots",
            "Stripes"
          ]
        },
        "primaryColor": {
          "type": "object",
          "description": "Primary pattern color {r, g, b, a}.",
          "additionalProperties": true
        },
        "secondaryColor": {
          "type": "object",
          "description": "Secondary pattern color {r, g, b, a}.",
          "additionalProperties": true
        },
        "tilesX": {
          "type": "number",
          "description": "Number of pattern tiles horizontally."
        },
        "tilesY": {
          "type": "number",
          "description": "Number of pattern tiles vertically."
        },
        "lineWidth": {
          "type": "number",
          "description": "Line width for grid/stripes (0-1)."
        },
        "brickRatio": {
          "type": "number",
          "description": "Width/height ratio for brick pattern."
        },
        "offset": {
          "type": "number",
          "description": "Brick offset ratio (0-1)."
        },
        "sourceTexture": {
          "type": "string",
          "description": "Source height map texture path."
        },
        "strength": {
          "type": "number",
          "description": "Strength or weight."
        },
        "algorithm": {
          "type": "string",
          "description": "Normal calculation algorithm.",
          "enum": [
            "Sobel",
            "Prewitt",
            "Scharr"
          ]
        },
        "flipY": {
          "type": "boolean",
          "description": "Flip green channel for DirectX/OpenGL compatibility."
        },
        "meshPath": {
          "type": "string",
          "description": "Mesh asset path."
        },
        "samples": {
          "type": "number",
          "description": "Number of AO samples."
        },
        "rayDistance": {
          "type": "number",
          "description": "Maximum ray distance for AO."
        },
        "bias": {
          "type": "number",
          "description": "AO bias to prevent self-occlusion."
        },
        "uvChannel": {
          "type": "number",
          "description": "UV channel to use for baking."
        },
        "filterMethod": {
          "type": "string",
          "description": "Resize filter method.",
          "enum": [
            "Nearest",
            "Bilinear",
            "Bicubic",
            "Lanczos"
          ]
        },
        "preserveAspect": {
          "type": "boolean",
          "description": "Preserve aspect ratio when resizing."
        },
        "outputPath": {
          "type": "string",
          "description": "Output file or directory path."
        },
        "inputBlackPoint": {
          "type": "number",
          "description": "Input black point (0-1)."
        },
        "inputWhitePoint": {
          "type": "number",
          "description": "Input white point (0-1)."
        },
        "gamma": {
          "type": "number",
          "description": "Gamma correction value."
        },
        "outputBlackPoint": {
          "type": "number",
          "description": "Output black point (0-1)."
        },
        "outputWhitePoint": {
          "type": "number",
          "description": "Output white point (0-1)."
        },
        "curvePoints": {
          "type": "array",
          "description": "Array of {x, y} curve control points.",
          "items": {
            "type": "object"
          }
        },
        "blurType": {
          "type": "string",
          "description": "Type of blur.",
          "enum": [
            "Gaussian",
            "Box",
            "Radial"
          ]
        },
        "sharpenType": {
          "type": "string",
          "description": "Type of sharpening.",
          "enum": [
            "UnsharpMask",
            "Laplacian"
          ]
        },
        "channel": {
          "type": "string",
          "description": "Target channel.",
          "enum": [
            "All",
            "Red",
            "Green",
            "Blue",
            "Alpha"
          ]
        },
        "invertAlpha": {
          "type": "boolean",
          "description": "Whether to invert alpha channel."
        },
        "amount": {
          "type": "number",
          "description": "Effect amount (0-1 for desaturate)."
        },
        "method": {
          "type": "string",
          "description": "Desaturation method.",
          "enum": [
            "Luminance",
            "Average",
            "Lightness"
          ]
        },
        "outputAsGrayscale": {
          "type": "boolean",
          "description": "Output extracted channel as grayscale."
        },
        "redChannel": {
          "type": "string",
          "description": "Source texture for red channel."
        },
        "greenChannel": {
          "type": "string",
          "description": "Source texture for green channel."
        },
        "blueChannel": {
          "type": "string",
          "description": "Source texture for blue channel."
        },
        "alphaChannel": {
          "type": "string",
          "description": "Source texture for alpha channel."
        },
        "redSourceChannel": {
          "type": "string",
          "description": "Which channel to use from red source.",
          "enum": [
            "Red",
            "Green",
            "Blue",
            "Alpha"
          ]
        },
        "greenSourceChannel": {
          "type": "string",
          "description": "Which channel to use from green source.",
          "enum": [
            "Red",
            "Green",
            "Blue",
            "Alpha"
          ]
        },
        "blueSourceChannel": {
          "type": "string",
          "description": "Which channel to use from blue source.",
          "enum": [
            "Red",
            "Green",
            "Blue",
            "Alpha"
          ]
        },
        "alphaSourceChannel": {
          "type": "string",
          "description": "Which channel to use from alpha source.",
          "enum": [
            "Red",
            "Green",
            "Blue",
            "Alpha"
          ]
        },
        "baseTexture": {
          "type": "string",
          "description": "Base texture path for combining."
        },
        "blendTexture": {
          "type": "string",
          "description": "Blend texture path for combining."
        },
        "blendMode": {
          "type": "string",
          "description": "Blend mode for combining textures.",
          "enum": [
            "Multiply",
            "Add",
            "Subtract",
            "Screen",
            "Overlay",
            "SoftLight",
            "HardLight",
            "Difference",
            "Normal"
          ]
        },
        "opacity": {
          "type": "number",
          "description": "Blend opacity (0-1)."
        },
        "maskTexture": {
          "type": "string",
          "description": "Optional mask texture for blending."
        },
        "compressionSettings": {
          "type": "string",
          "description": "Texture compression setting.",
          "enum": [
            "TC_Default",
            "TC_Normalmap",
            "TC_Masks",
            "TC_Grayscale",
            "TC_Displacementmap",
            "TC_VectorDisplacementmap",
            "TC_HDR",
            "TC_EditorIcon",
            "TC_Alpha",
            "TC_DistanceFieldFont",
            "TC_HDR_Compressed",
            "TC_BC7"
          ]
        },
        "textureGroup": {
          "type": "string",
          "description": "Texture group (TEXTUREGROUP_World, TEXTUREGROUP_Character, TEXTUREGROUP_UI, etc.)."
        },
        "lodBias": {
          "type": "number",
          "description": "LOD bias (-2 to 4, lower = higher quality)."
        },
        "virtualTextureStreaming": {
          "type": "boolean",
          "description": "Enable virtual texture streaming."
        },
        "tileSize": {
          "type": "number",
          "description": "Virtual texture tile size (32, 64, 128, 256, 512, 1024)."
        },
        "tileBorderSize": {
          "type": "number",
          "description": "Virtual texture tile border size."
        },
        "neverStream": {
          "type": "boolean",
          "description": "Disable texture streaming."
        },
        "streamingPriority": {
          "type": "number",
          "description": "Streaming priority (-1 to 1, lower = higher priority)."
        },
        "hdr": {
          "type": "boolean",
          "description": "Create HDR texture (16-bit float)."
        },
        "save": {
          "type": "boolean",
          "description": "Save the asset(s) after the operation."
        }
      },
      "required": [
        "action"
      ]
    }
  },
  {
    "name": "manage_tools",
    "description": "Dynamic MCP tool management. Enable/disable tools and categories at runtime. Actions: list_tools, list_categories, enable_tools, disable_tools, enable_category, disable_category, get_status, reset.",
    "category": "core",
    "inputSchema": {
      "type": "object",
      "properties": {
        "action": {
          "type": "string",
          "description": "list_tools: show all tools with status. list_categories: show categories. enable/disable_tools: toggle specific tools. enable/disable_category: toggle category. get_status: current state. reset: restore defaults.",
          "enum": [
            "list_tools",
            "list_categories",
            "enable_tools",
            "disable_tools",
            "enable_category",
            "disable_category",
            "get_status",
            "reset"
          ]
        },
        "tools": {
          "type": "array",
          "description": "Tool names to enable/disable",
          "items": {
            "type": "string"
          }
        },
        "category": {
          "type": "string",
          "description": "Category name to enable/disable (core, world, authoring, gameplay, utility, all)"
        }
      },
      "required": [
        "action"
      ]
    }
  },
  {
    "name": "manage_volumes",
    "description": "Create trigger volumes, blocking volumes, physics volumes, audio volumes, and navigation bounds.",
    "category": "world",
    "inputSchema": {
      "type": "object",
      "properties": {
        "action": {
          "type": "string",
          "description": "Volume action to perform",
          "enum": [
            "create_trigger_volume",
            "add_trigger_volume",
            "create_trigger_box",
            "create_trigger_sphere",
            "create_trigger_capsule",
            "create_blocking_volume",
            "add_blocking_volume",
            "create_kill_z_volume",
            "add_kill_z_volume",
            "create_pain_causing_volume",
            "create_physics_volume",
            "add_physics_volume",
            "create_audio_volume",
            "create_reverb_volume",
            "create_cull_distance_volume",
            "add_cull_distance_volume",
            "create_precomputed_visibility_volume",
            "create_lightmass_importance_volume",
            "create_nav_mesh_bounds_volume",
            "create_nav_modifier_volume",
            "create_camera_blocking_volume",
            "create_post_process_volume",
            "add_post_process_volume",
            "set_volume_extent",
            "set_volume_bounds",
            "set_volume_properties",
            "remove_volume",
            "get_volumes_info"
          ]
        },
        "volumeName": {
          "type": "string",
          "description": "Name of the volume."
        },
        "volumePath": {
          "type": "string",
          "description": "Path to volume."
        },
        "actorPath": {
          "type": "string",
          "description": "Path to actor."
        },
        "location": {
          "type": "object",
          "description": "World location for the volume.",
          "properties": {
            "x": {
              "type": "number"
            },
            "y": {
              "type": "number"
            },
            "z": {
              "type": "number"
            }
          }
        },
        "rotation": {
          "type": "object",
          "description": "Rotation of the volume.",
          "properties": {
            "pitch": {
              "type": "number"
            },
            "yaw": {
              "type": "number"
            },
            "roll": {
              "type": "number"
            }
          }
        },
        "extent": {
          "type": "object",
          "description": "Extent (half-size) of the volume in each axis.",
          "properties": {
            "x": {
              "type": "number"
            },
            "y": {
              "type": "number"
            },
            "z": {
              "type": "number"
            }
          }
        },
        "sphereRadius": {
          "type": "number",
          "description": "Radius for sphere trigger volumes."
        },
        "capsuleRadius": {
          "type": "number",
          "description": "Radius for capsule trigger volumes."
        },
        "capsuleHalfHeight": {
          "type": "number",
          "description": "Half-height for capsule trigger volumes."
        },
        "boxExtent": {
          "type": "object",
          "description": "Extent for box trigger volumes.",
          "properties": {
            "x": {
              "type": "number"
            },
            "y": {
              "type": "number"
            },
            "z": {
              "type": "number"
            }
          }
        },
        "bPainCausing": {
          "type": "boolean",
          "description": "Whether the volume causes pain/damage."
        },
        "damagePerSec": {
          "type": "number",
          "description": "Damage per second for pain volumes."
        },
        "damageType": {
          "type": "string",
          "description": "Damage type class path for pain volumes."
        },
        "bWaterVolume": {
          "type": "boolean",
          "description": "Whether this is a water volume."
        },
        "fluidFriction": {
          "type": "number",
          "description": "Fluid friction for physics volumes."
        },
        "terminalVelocity": {
          "type": "number",
          "description": "Terminal velocity in the volume."
        },
        "priority": {
          "type": "number",
          "description": "Priority value."
        },
        "bEnabled": {
          "type": "boolean",
          "description": "Whether the audio volume is enabled."
        },
        "reverbEffect": {
          "type": "string",
          "description": "Reverb effect asset path."
        },
        "reverbVolume": {
          "type": "number",
          "description": "Volume level for reverb (0.0-1.0)."
        },
        "fadeTime": {
          "type": "number",
          "description": "Fade time in seconds."
        },
        "cullDistances": {
          "type": "array",
          "description": "Array of size/distance pairs for cull distance volumes.",
          "items": {
            "type": "object"
          }
        },
        "areaClass": {
          "type": "string",
          "description": "Navigation area class path."
        },
        "bDynamicModifier": {
          "type": "boolean",
          "description": "Whether nav modifier updates dynamically."
        },
        "bUnbound": {
          "type": "boolean",
          "description": "Whether post process volume affects entire world."
        },
        "blendRadius": {
          "type": "number",
          "description": "Blend radius for post process volume."
        },
        "blendWeight": {
          "type": "number",
          "description": "Blend weight (0.0-1.0) for post process."
        },
        "properties": {
          "type": "object",
          "description": "Additional volume-specific properties as key-value pairs.",
          "additionalProperties": true
        },
        "filter": {
          "type": "string",
          "description": "General search filter."
        },
        "volumeType": {
          "type": "string",
          "description": "Type filter for get_volumes_info (e.g., \"Trigger\", \"Physics\")."
        },
        "save": {
          "type": "boolean",
          "description": "Save the asset(s) after the operation."
        }
      },
      "required": [
        "action"
      ]
    }
  },
  {
    "name": "manage_widget_authoring",
    "description": "Create UMG widgets: buttons, text, images, sliders. Configure layouts, bindings, animations. Build HUDs and menus.",
    "category": "authoring",
    "inputSchema": {
      "type": "object",
      "properties": {
        "action": {
          "type": "string",
          "description": "The widget authoring action to perform.",
          "enum": [
            "create_widget_blueprint",
            "set_widget_parent_class",
            "add_canvas_panel",
            "add_horizontal_box",
            "add_vertical_box",
            "add_overlay",
            "add_grid_panel",
            "add_uniform_grid",
            "add_wrap_box",
            "add_scroll_box",
            "add_size_box",
            "add_scale_box",
            "add_border",
            "add_text_block",
            "add_rich_text_block",
            "add_image",
            "add_button",
            "add_check_box",
            "add_slider",
            "add_progress_bar",
            "add_text_input",
            "add_combo_box",
            "add_spin_box",
            "add_list_view",
            "add_tree_view",
            "set_anchor",
            "set_alignment",
            "set_position",
            "set_size",
            "set_padding",
            "set_z_order",
            "set_render_transform",
            "set_visibility",
            "set_style",
            "set_clipping",
            "create_property_binding",
            "bind_text",
            "bind_visibility",
            "bind_color",
            "bind_enabled",
            "bind_on_clicked",
            "bind_on_hovered",
            "bind_on_value_changed",
            "create_widget_animation",
            "add_animation_track",
            "add_animation_keyframe",
            "set_animation_loop",
            "create_main_menu",
            "create_pause_menu",
            "create_settings_menu",
            "create_loading_screen",
            "create_hud_widget",
            "add_health_bar",
            "add_ammo_counter",
            "add_minimap",
            "add_crosshair",
            "add_compass",
            "add_interaction_prompt",
            "add_objective_tracker",
            "add_damage_indicator",
            "create_inventory_ui",
            "create_dialog_widget",
            "create_radial_menu",
            "get_widget_info",
            "preview_widget"
          ]
        },
        "name": {
          "type": "string",
          "description": "Name identifier."
        },
        "folder": {
          "type": "string",
          "description": "Path to a directory."
        },
        "widgetPath": {
          "type": "string",
          "description": "Widget blueprint path."
        },
        "slotName": {
          "type": "string",
          "description": "Name of the slot."
        },
        "parentSlot": {
          "type": "string",
          "description": "Parent slot to add widget to."
        },
        "parentClass": {
          "type": "string",
          "description": "Path or name of the parent class."
        },
        "anchorMin": {
          "type": "object",
          "description": "Minimum anchor point (0-1).",
          "properties": {
            "x": {
              "type": "number"
            },
            "y": {
              "type": "number"
            }
          }
        },
        "anchorMax": {
          "type": "object",
          "description": "Maximum anchor point (0-1).",
          "properties": {
            "x": {
              "type": "number"
            },
            "y": {
              "type": "number"
            }
          }
        },
        "alignment": {
          "type": "object",
          "description": "Widget alignment (0-1).",
          "properties": {
            "x": {
              "type": "number"
            },
            "y": {
              "type": "number"
            }
          }
        },
        "alignmentX": {
          "type": "number",
          "description": "Horizontal alignment (0-1)."
        },
        "alignmentY": {
          "type": "number",
          "description": "Vertical alignment (0-1)."
        },
        "positionX": {
          "type": "number",
          "description": "X position."
        },
        "positionY": {
          "type": "number",
          "description": "Y position."
        },
        "sizeX": {
          "type": "number",
          "description": "Width."
        },
        "sizeY": {
          "type": "number",
          "description": "Height."
        },
        "sizeToContent": {
          "type": "boolean",
          "description": "Size to content."
        },
        "left": {
          "type": "number",
          "description": "Left padding."
        },
        "top": {
          "type": "number",
          "description": "Top padding."
        },
        "right": {
          "type": "number",
          "description": "Right padding."
        },
        "bottom": {
          "type": "number",
          "description": "Bottom padding."
        },
        "zOrder": {
          "type": "number",
          "description": "Z-order for canvas slot."
        },
        "translation": {
          "type": "object",
          "description": "Render translation.",
          "properties": {
            "x": {
              "type": "number"
            },
            "y": {
              "type": "number"
            }
          }
        },
        "scale": {
          "type": "object",
          "description": "Render scale.",
          "properties": {
            "x": {
              "type": "number"
            },
            "y": {
              "type": "number"
            }
          }
        },
        "shear": {
          "type": "object",
          "description": "Render shear.",
          "properties": {
            "x": {
              "type": "number"
            },
            "y": {
              "type": "number"
            }
          }
        },
        "angle": {
          "type": "number",
          "description": "Angle in degrees."
        },
        "pivot": {
          "type": "object",
          "description": "Rotation/scale pivot.",
          "properties": {
            "x": {
              "type": "number"
            },
            "y": {
              "type": "number"
            }
          }
        },
        "visibility": {
          "type": "string",
          "description": "Widget visibility state.",
          "enum": [
            "Visible",
            "Collapsed",
            "Hidden",
            "HitTestInvisible",
            "SelfHitTestInvisible"
          ]
        },
        "clipping": {
          "type": "string",
          "description": "Widget clipping mode.",
          "enum": [
            "Inherit",
            "ClipToBounds",
            "ClipToBoundsWithoutIntersecting",
            "ClipToBoundsAlways",
            "OnDemand"
          ]
        },
        "text": {
          "type": "string",
          "description": "Text content."
        },
        "font": {
          "type": "string",
          "description": "Font asset path."
        },
        "fontSize": {
          "type": "number",
          "description": "Font size."
        },
        "colorAndOpacity": {
          "type": "object",
          "description": "Color and opacity (0-1 values).",
          "properties": {
            "r": {
              "type": "number"
            },
            "g": {
              "type": "number"
            },
            "b": {
              "type": "number"
            },
            "a": {
              "type": "number"
            }
          }
        },
        "justification": {
          "type": "string",
          "description": "Text justification.",
          "enum": [
            "Left",
            "Center",
            "Right"
          ]
        },
        "autoWrap": {
          "type": "boolean",
          "description": "Enable text auto-wrap."
        },
        "texturePath": {
          "type": "string",
          "description": "Texture asset path."
        },
        "brushSize": {
          "type": "object",
          "description": "Brush/image size.",
          "properties": {
            "x": {
              "type": "number"
            },
            "y": {
              "type": "number"
            }
          }
        },
        "brushTiling": {
          "type": "string",
          "description": "Image tiling mode.",
          "enum": [
            "NoTile",
            "Horizontal",
            "Vertical",
            "Both"
          ]
        },
        "isEnabled": {
          "type": "boolean",
          "description": "Widget enabled state."
        },
        "isChecked": {
          "type": "boolean",
          "description": "Checkbox checked state."
        },
        "value": {
          "type": "number",
          "description": "Slider/spinbox value."
        },
        "minValue": {
          "type": "number",
          "description": "Minimum value."
        },
        "maxValue": {
          "type": "number",
          "description": "Maximum value."
        },
        "stepSize": {
          "type": "number",
          "description": "Value step size."
        },
        "delta": {
          "type": "number",
          "description": "Spinbox increment."
        },
        "percent": {
          "type": "number",
          "description": "Progress bar percentage (0-1)."
        },
        "fillColorAndOpacity": {
          "type": "object",
          "description": "Fill color for progress bar.",
          "properties": {
            "r": {
              "type": "number"
            },
            "g": {
              "type": "number"
            },
            "b": {
              "type": "number"
            },
            "a": {
              "type": "number"
            }
          }
        },
        "barFillType": {
          "type": "string",
          "description": "Progress bar fill direction.",
          "enum": [
            "LeftToRight",
            "RightToLeft",
            "TopToBottom",
            "BottomToTop",
            "FillFromCenter"
          ]
        },
        "isMarquee": {
          "type": "boolean",
          "description": "Progress bar marquee mode."
        },
        "inputType": {
          "type": "string",
          "description": "Text input type.",
          "enum": [
            "single",
            "multi"
          ]
        },
        "hintText": {
          "type": "string",
          "description": "Placeholder hint text."
        },
        "isPassword": {
          "type": "boolean",
          "description": "Password masking."
        },
        "options": {
          "type": "array",
          "description": "Combo box options.",
          "items": {
            "type": "string"
          }
        },
        "selectedOption": {
          "type": "string",
          "description": "Selected combo box option."
        },
        "entryWidgetClass": {
          "type": "string",
          "description": "List/tree view entry widget class."
        },
        "orientation": {
          "type": "string",
          "description": "Widget orientation.",
          "enum": [
            "Horizontal",
            "Vertical"
          ]
        },
        "selectionMode": {
          "type": "string",
          "description": "Selection mode for list/tree.",
          "enum": [
            "None",
            "Single",
            "Multi"
          ]
        },
        "scrollBarVisibility": {
          "type": "string",
          "description": "Scroll bar visibility.",
          "enum": [
            "Visible",
            "Collapsed",
            "Auto"
          ]
        },
        "alwaysShowScrollbar": {
          "type": "boolean",
          "description": "Always show scrollbar."
        },
        "columnCount": {
          "type": "number",
          "description": "Number of columns."
        },
        "rowCount": {
          "type": "number",
          "description": "Number of rows."
        },
        "slotPadding": {
          "type": "number",
          "description": "Padding between slots."
        },
        "minDesiredSlotWidth": {
          "type": "number",
          "description": "Minimum slot width."
        },
        "minDesiredSlotHeight": {
          "type": "number",
          "description": "Minimum slot height."
        },
        "innerSlotPadding": {
          "type": "number",
          "description": "Inner slot padding."
        },
        "wrapWidth": {
          "type": "number",
          "description": "Wrap width for wrap box."
        },
        "explicitWrapWidth": {
          "type": "boolean",
          "description": "Use explicit wrap width."
        },
        "widthOverride": {
          "type": "number",
          "description": "Width override for size box."
        },
        "heightOverride": {
          "type": "number",
          "description": "Height override for size box."
        },
        "minDesiredWidth": {
          "type": "number",
          "description": "Minimum desired width."
        },
        "minDesiredHeight": {
          "type": "number",
          "description": "Minimum desired height."
        },
        "stretch": {
          "type": "string",
          "description": "Scale box stretch mode.",
          "enum": [
            "None",
            "Fill",
            "ScaleToFit",
            "ScaleToFitX",
            "ScaleToFitY",
            "ScaleToFill",
            "UserSpecified"
          ]
        },
        "stretchDirection": {
          "type": "string",
          "description": "Scale box stretch direction.",
          "enum": [
            "Both",
            "DownOnly",
            "UpOnly"
          ]
        },
        "userSpecifiedScale": {
          "type": "number",
          "description": "User specified scale value."
        },
        "brushColor": {
          "type": "object",
          "description": "Border brush color.",
          "properties": {
            "r": {
              "type": "number"
            },
            "g": {
              "type": "number"
            },
            "b": {
              "type": "number"
            },
            "a": {
              "type": "number"
            }
          }
        },
        "padding": {
          "type": "number",
          "description": "Uniform padding."
        },
        "horizontalAlignment": {
          "type": "string",
          "description": "Horizontal alignment.",
          "enum": [
            "Fill",
            "Left",
            "Center",
            "Right"
          ]
        },
        "verticalAlignment": {
          "type": "string",
          "description": "Vertical alignment.",
          "enum": [
            "Fill",
            "Top",
            "Center",
            "Bottom"
          ]
        },
        "color": {
          "type": "object",
          "description": "Widget color.",
          "properties": {
            "r": {
              "type": "number"
            },
            "g": {
              "type": "number"
            },
            "b": {
              "type": "number"
            },
            "a": {
              "type": "number"
            }
          }
        },
        "opacity": {
          "type": "number",
          "description": "Widget opacity (0-1)."
        },
        "brush": {
          "type": "string",
          "description": "Brush asset path."
        },
        "backgroundImage": {
          "type": "string",
          "description": "Background image path."
        },
        "style": {
          "type": "string",
          "description": "Style preset name."
        },
        "propertyName": {
          "type": "string",
          "description": "Name of the property."
        },
        "bindingType": {
          "type": "string",
          "description": "Binding type.",
          "enum": [
            "function",
            "variable"
          ]
        },
        "bindingSource": {
          "type": "string",
          "description": "Variable or function name to bind to."
        },
        "functionName": {
          "type": "string",
          "description": "Name of the function."
        },
        "onHoveredFunction": {
          "type": "string",
          "description": "Function to call on hover."
        },
        "onUnhoveredFunction": {
          "type": "string",
          "description": "Function to call on unhover."
        },
        "animationName": {
          "type": "string",
          "description": "Animation name."
        },
        "length": {
          "type": "number",
          "description": "Animation length in seconds."
        },
        "trackType": {
          "type": "string",
          "description": "Animation track type.",
          "enum": [
            "transform",
            "color",
            "opacity",
            "renderOpacity",
            "material"
          ]
        },
        "time": {
          "type": "number",
          "description": "Keyframe time."
        },
        "interpolation": {
          "type": "string",
          "description": "Keyframe interpolation.",
          "enum": [
            "linear",
            "cubic",
            "constant"
          ]
        },
        "loopCount": {
          "type": "number",
          "description": "Number of loops (-1 for infinite)."
        },
        "playMode": {
          "type": "string",
          "description": "Animation play mode.",
          "enum": [
            "forward",
            "reverse",
            "pingpong"
          ]
        },
        "includePlayButton": {
          "type": "boolean",
          "description": "Include play button in menu."
        },
        "includeSettingsButton": {
          "type": "boolean",
          "description": "Include settings button."
        },
        "includeQuitButton": {
          "type": "boolean",
          "description": "Include quit button."
        },
        "includeResumeButton": {
          "type": "boolean",
          "description": "Include resume button."
        },
        "includeQuitToMenuButton": {
          "type": "boolean",
          "description": "Include quit to menu button."
        },
        "settingsType": {
          "type": "string",
          "description": "Settings menu type.",
          "enum": [
            "video",
            "audio",
            "controls",
            "gameplay",
            "all"
          ]
        },
        "includeApplyButton": {
          "type": "boolean",
          "description": "Include apply button."
        },
        "includeResetButton": {
          "type": "boolean",
          "description": "Include reset button."
        },
        "includeProgressBar": {
          "type": "boolean",
          "description": "Include progress bar."
        },
        "includeTipText": {
          "type": "boolean",
          "description": "Include tip text."
        },
        "includeBackgroundImage": {
          "type": "boolean",
          "description": "Include background image."
        },
        "titleText": {
          "type": "string",
          "description": "Menu title text."
        },
        "elements": {
          "type": "array",
          "description": "HUD elements to include.",
          "items": {
            "type": "string"
          }
        },
        "barStyle": {
          "type": "string",
          "description": "Health bar style.",
          "enum": [
            "simple",
            "segmented",
            "radial"
          ]
        },
        "showNumbers": {
          "type": "boolean",
          "description": "Show numeric values."
        },
        "barColor": {
          "type": "object",
          "description": "Bar color.",
          "properties": {
            "r": {
              "type": "number"
            },
            "g": {
              "type": "number"
            },
            "b": {
              "type": "number"
            },
            "a": {
              "type": "number"
            }
          }
        },
        "ammoStyle": {
          "type": "string",
          "description": "Ammo counter style.",
          "enum": [
            "numeric",
            "icon"
          ]
        },
        "showReserve": {
          "type": "boolean",
          "description": "Show reserve ammo."
        },
        "ammoIcon": {
          "type": "string",
          "description": "Ammo icon texture."
        },
        "minimapSize": {
          "type": "number",
          "description": "Minimap size."
        },
        "minimapShape": {
          "type": "string",
          "description": "Minimap shape.",
          "enum": [
            "circle",
            "square"
          ]
        },
        "rotateWithPlayer": {
          "type": "boolean",
          "description": "Rotate minimap with player."
        },
        "showObjectives": {
          "type": "boolean",
          "description": "Show objectives on minimap."
        },
        "crosshairStyle": {
          "type": "string",
          "description": "Crosshair style.",
          "enum": [
            "dot",
            "cross",
            "circle",
            "custom"
          ]
        },
        "crosshairSize": {
          "type": "number",
          "description": "Crosshair size."
        },
        "spreadMultiplier": {
          "type": "number",
          "description": "Crosshair spread multiplier."
        },
        "showDegrees": {
          "type": "boolean",
          "description": "Show compass degrees."
        },
        "showCardinals": {
          "type": "boolean",
          "description": "Show cardinal directions."
        },
        "promptFormat": {
          "type": "string",
          "description": "Interaction prompt format."
        },
        "showKeyIcon": {
          "type": "boolean",
          "description": "Show key icon in prompt."
        },
        "keyIconStyle": {
          "type": "string",
          "description": "Key icon style."
        },
        "maxVisibleObjectives": {
          "type": "number",
          "description": "Maximum visible objectives."
        },
        "showProgress": {
          "type": "boolean",
          "description": "Show objective progress."
        },
        "animateUpdates": {
          "type": "boolean",
          "description": "Animate objective updates."
        },
        "indicatorStyle": {
          "type": "string",
          "description": "Damage indicator style.",
          "enum": [
            "directional",
            "vignette",
            "both"
          ]
        },
        "fadeTime": {
          "type": "number",
          "description": "Fade time in seconds."
        },
        "gridSize": {
          "type": "object",
          "description": "Inventory grid size.",
          "properties": {
            "columns": {
              "type": "number"
            },
            "rows": {
              "type": "number"
            }
          }
        },
        "slotSize": {
          "type": "number",
          "description": "Inventory slot size."
        },
        "showEquipment": {
          "type": "boolean",
          "description": "Show equipment panel."
        },
        "showDetails": {
          "type": "boolean",
          "description": "Show item details panel."
        },
        "showPortrait": {
          "type": "boolean",
          "description": "Show speaker portrait."
        },
        "showSpeakerName": {
          "type": "boolean",
          "description": "Show speaker name."
        },
        "choiceLayout": {
          "type": "string",
          "description": "Dialog choice layout.",
          "enum": [
            "vertical",
            "horizontal",
            "radial"
          ]
        },
        "segmentCount": {
          "type": "number",
          "description": "Number of radial segments."
        },
        "innerRadius": {
          "type": "number",
          "description": "Inner radius of radial menu."
        },
        "outerRadius": {
          "type": "number",
          "description": "Outer radius of radial menu."
        },
        "showIcons": {
          "type": "boolean",
          "description": "Show icons in radial menu."
        },
        "showLabels": {
          "type": "boolean",
          "description": "Show labels in radial menu."
        },
        "previewSize": {
          "type": "string",
          "description": "Preview resolution preset.",
          "enum": [
            "1080p",
            "720p",
            "mobile",
            "custom"
          ]
        },
        "customWidth": {
          "type": "number",
          "description": "Custom preview width."
        },
        "customHeight": {
          "type": "number",
          "description": "Custom preview height."
        }
      },
      "required": [
        "action"
      ]
    }
  },
  {
    "name": "system_control",
    "description": "Run profiling, set quality/CVars, execute console commands, execute Python scripts, run UBT, and manage widgets.",
    "category": "core",
    "inputSchema": {
      "type": "object",
      "properties": {
        "action": {
          "type": "string",
          "description": "Action",
          "enum": [
            "profile",
            "show_fps",
            "set_quality",
            "screenshot",
            "set_resolution",
            "set_fullscreen",
            "execute_command",
            "console_command",
            "run_ubt",
            "run_tests",
            "subscribe",
            "unsubscribe",
            "spawn_category",
            "start_session",
            "lumen_update_scene",
            "play_sound",
            "create_widget",
            "show_widget",
            "add_widget_child",
            "set_cvar",
            "get_project_settings",
            "validate_assets",
            "set_project_setting",
            "execute_python"
          ]
        },
        "profileType": {
          "type": "string"
        },
        "category": {
          "type": "string"
        },
        "level": {
          "type": "number"
        },
        "enabled": {
          "type": "boolean",
          "description": "Whether the item/feature is enabled."
        },
        "resolution": {
          "type": "string",
          "description": "Resolution setting (e.g., 1024x1024)."
        },
        "command": {
          "type": "string"
        },
        "target": {
          "type": "string"
        },
        "platform": {
          "type": "string"
        },
        "configuration": {
          "type": "string"
        },
        "arguments": {
          "type": "string"
        },
        "filter": {
          "type": "string"
        },
        "channels": {
          "type": "string"
        },
        "widgetPath": {
          "type": "string",
          "description": "Widget blueprint path."
        },
        "childClass": {
          "type": "string"
        },
        "parentName": {
          "type": "string"
        },
        "section": {
          "type": "string"
        },
        "key": {
          "type": "string"
        },
        "value": {
          "type": "string"
        },
        "configName": {
          "type": "string"
        },
        "code": {
          "type": "string",
          "description": "Python code to execute inline"
        },
        "file": {
          "type": "string",
          "description": "Path to .py file to execute"
        }
      },
      "required": [
        "action"
      ]
    }
  }
];
