#include "geometric_validator.h"
#include "utils.h" // For logging
#include <geos_c.h> // GEOS C API
#include <stdlib.h>
#include <string.h>

// Global GEOS context handle
static GEOSContextHandle_t geos_context_handle = NULL;

// Forward declarations for helper functions
static GEOSGeometry* json_to_geos(json_t* geometry_json, GEOSContextHandle_t ctx);
static json_t* geos_to_json(const GEOSGeometry* geos_geom, GEOSContextHandle_t ctx);
// static GEOSGeometry* json_to_geos_point(json_t* geometry_json, GEOSContextHandle_t ctx); // Now part of json_to_geos
// static GEOSGeometry* json_to_geos_linestring(json_t* geometry_json, GEOSContextHandle_t ctx); // Now part of json_to_geos
// static GEOSGeometry* json_to_geos_polygon(json_t* geometry_json, GEOSContextHandle_t ctx); // Now part of json_to_geos
static GEOSGeometry* json_to_geos_single_or_multi(json_t* geometry_json, GEOSContextHandle_t ctx, const char* type_str);
static GEOSGeometry* json_to_geos_geometrycollection(json_t* geometry_json, GEOSContextHandle_t ctx);


// static json_t* geos_to_json_point(const GEOSGeometry* geos_geom, GEOSContextHandle_t ctx); // Now part of geos_to_json
// static json_t* geos_to_json_linestring(const GEOSGeometry* geos_geom, GEOSContextHandle_t ctx); // Now part of geos_to_json
// static json_t* geos_to_json_polygon(const GEOSGeometry* geos_geom, GEOSContextHandle_t ctx); // Now part of geos_to_json
static json_t* geos_to_json_multi(const GEOSGeometry* geos_geom, GEOSContextHandle_t ctx, const char* geojson_type_str);
static json_t* geos_to_json_geometrycollection(const GEOSGeometry* geos_geom, GEOSContextHandle_t ctx);


// GEOS message handler (simple one for logging)
static void geos_message_handler(const char *message, void *userdata) {
    LOG_WARN("GEOS: %s", message);
    (void)userdata; // Unused
}

int geometric_validator_init() {
    if (geos_context_handle != NULL) {
        LOG_WARN("Geometric validator already initialized.");
        return 0;
    }
    geos_context_handle = GEOS_init_r();
    if (geos_context_handle == NULL) {
        LOG_ERROR("Failed to initialize GEOS context.");
        return -1;
    }
    LOG_INFO("GEOS context initialized successfully.");
    return 0;
}

void geometric_validator_shutdown() {
    if (geos_context_handle != NULL) {
        GEOS_finish_r(geos_context_handle);
        geos_context_handle = NULL;
        LOG_INFO("GEOS context finished.");
    }
}

int load_geometric_validator_config(const SpindexConfig* global_config, GeometricValidatorConfig* val_config) {
    if (!global_config || !val_config) {
        LOG_ERROR("GEO_VALIDATOR: Null config provided.");
        return -1;
    }
    val_config->enabled = global_config->geometric_validator_enabled;
    val_config->enable_repair = global_config->geometric_validator_enable_repair;
    LOG_DEBUG("Geometric Validator Config: Enabled=%d, EnableRepair=%d", val_config->enabled, val_config->enable_repair);
    return 0;
}

static GEOSCoordSequence* json_coordinates_to_geos_coord_seq(json_t* coordinates_json, GEOSContextHandle_t ctx, int min_points, int is_ring) {
    if (!json_is_array(coordinates_json)) {
        LOG_WARN("Coordinates is not a JSON array.");
        return NULL;
    }
    size_t num_coords = json_array_size(coordinates_json);
    if ((int)num_coords < min_points) {
        LOG_WARN("Not enough coordinates for type: got %zu, need %d.", num_coords, min_points);
        return NULL;
    }
    if (is_ring && num_coords > 0) {
        json_t* first_pt_json = json_array_get(coordinates_json, 0);
        json_t* last_pt_json = json_array_get(coordinates_json, num_coords - 1);
        if (!json_is_array(first_pt_json) || json_array_size(first_pt_json) < 2 ||
            !json_is_array(last_pt_json) || json_array_size(last_pt_json) < 2 ||
            json_real_value(json_array_get(first_pt_json, 0)) != json_real_value(json_array_get(last_pt_json, 0)) ||
            json_real_value(json_array_get(first_pt_json, 1)) != json_real_value(json_array_get(last_pt_json, 1))) {
            LOG_WARN("Ring is not closed (first and last points differ).");
        }
    }

    GEOSCoordSequence* seq = GEOSCoordSeq_create_r(ctx, num_coords, 2);
    if (!seq) {
        LOG_ERROR("Failed to create GEOSCoordSequence.");
        return NULL;
    }
    for (size_t i = 0; i < num_coords; ++i) {
        json_t* point_json = json_array_get(coordinates_json, i);
        if (!json_is_array(point_json) || json_array_size(point_json) < 2) {
            LOG_WARN("Coordinate entry %zu is not a valid point array.", i);
            GEOSCoordSeq_destroy_r(ctx, seq); return NULL;
        }
        if (GEOSCoordSeq_setX_r(ctx, seq, i, json_real_value(json_array_get(point_json, 0))) == 0 ||
            GEOSCoordSeq_setY_r(ctx, seq, i, json_real_value(json_array_get(point_json, 1))) == 0) {
            LOG_ERROR("Failed to set coordinates in GEOSCoordSequence.");
            GEOSCoordSeq_destroy_r(ctx, seq); return NULL;
        }
    }
    return seq;
}

// Generic function to handle single geometries (Point, LineString, Polygon)
// and their Multi* counterparts.
static GEOSGeometry* json_to_geos_single_or_multi(json_t* geometry_json, GEOSContextHandle_t ctx, const char* type_str) {
    GEOSGeomTypes target_geos_type;
    int is_multi = 0;
    const char* single_type_str = type_str;

    if (strncmp(type_str, "Multi", 5) == 0) {
        is_multi = 1;
        single_type_str = type_str + 5; // Skip "Multi"
    }

    if (strcmp(single_type_str, "Point") == 0) target_geos_type = GEOS_POINT;
    else if (strcmp(single_type_str, "LineString") == 0) target_geos_type = GEOS_LINESTRING;
    else if (strcmp(single_type_str, "Polygon") == 0) target_geos_type = GEOS_POLYGON;
    else {
        LOG_WARN("json_to_geos_single_or_multi: Unknown single type: %s", single_type_str);
        return NULL;
    }

    if (!is_multi) { // Handle single geometries
        json_t* coords_json = json_object_get(geometry_json, "coordinates");
        if (target_geos_type == GEOS_POINT) {
            GEOSCoordSequence* seq = json_coordinates_to_geos_coord_seq(coords_json, ctx, 1, 0);
            return seq ? GEOSGeom_createPoint_r(ctx, seq) : NULL;
        }
        if (target_geos_type == GEOS_LINESTRING) {
            GEOSCoordSequence* seq = json_coordinates_to_geos_coord_seq(coords_json, ctx, 2, 0);
            return seq ? GEOSGeom_createLineString_r(ctx, seq) : NULL;
        }
        if (target_geos_type == GEOS_POLYGON) {
            if (!json_is_array(coords_json) || json_array_size(coords_json) == 0) {
                LOG_WARN("Polygon coordinates is not an array or is empty."); return NULL;
            }
            json_t* exterior_ring_coords_json = json_array_get(coords_json, 0);
            GEOSCoordSequence* exterior_seq = json_coordinates_to_geos_coord_seq(exterior_ring_coords_json, ctx, 4, 1);
            if (!exterior_seq) { LOG_WARN("Failed to create exterior ring for polygon."); return NULL; }
            GEOSGeometry* exterior_ring = GEOSGeom_createLinearRing_r(ctx, exterior_seq);
            if (!exterior_ring) { LOG_ERROR("Failed to create GEOSLinearRing for exterior shell."); return NULL; }

            size_t num_interior_rings = json_array_size(coords_json) - 1;
            GEOSGeometry** interior_rings_array = NULL;
            if (num_interior_rings > 0) {
                interior_rings_array = (GEOSGeometry**)malloc(num_interior_rings * sizeof(GEOSGeometry*));
                if (!interior_rings_array) {
                    LOG_ERROR("Failed to allocate for interior rings."); GEOSGeom_destroy_r(ctx, exterior_ring); return NULL;
                }
                for (size_t i = 0; i < num_interior_rings; ++i) {
                    json_t* interior_coords_json = json_array_get(coords_json, i + 1);
                    GEOSCoordSequence* interior_seq = json_coordinates_to_geos_coord_seq(interior_coords_json, ctx, 4, 1);
                    if (!interior_seq) {
                        LOG_WARN("Failed for interior ring %zu.", i); GEOSGeom_destroy_r(ctx, exterior_ring);
                        for (size_t j = 0; j < i; ++j) GEOSGeom_destroy_r(ctx, interior_rings_array[j]);
                        free(interior_rings_array); return NULL;
                    }
                    interior_rings_array[i] = GEOSGeom_createLinearRing_r(ctx, interior_seq);
                    if (!interior_rings_array[i]) {
                        LOG_ERROR("Failed for interior ring GEOSLinearRing %zu.", i); GEOSGeom_destroy_r(ctx, exterior_ring);
                        for (size_t j = 0; j < i; ++j) GEOSGeom_destroy_r(ctx, interior_rings_array[j]);
                        free(interior_rings_array); return NULL;
                    }
                }
            }
            GEOSGeometry* polygon = GEOSGeom_createPolygon_r(ctx, exterior_ring, interior_rings_array, num_interior_rings);
            // GEOSGeom_createPolygon_r consumes exterior_ring and elements of interior_rings_array even on failure, if they were valid.
            // The array itself needs to be freed if it was allocated.
            if (interior_rings_array) free(interior_rings_array);
            return polygon;
        }
    } else { // Handle Multi* geometries
        GEOSGeomTypes collection_type;
        if (target_geos_type == GEOS_POINT) collection_type = GEOS_MULTIPOINT;
        else if (target_geos_type == GEOS_LINESTRING) collection_type = GEOS_MULTILINESTRING;
        else if (target_geos_type == GEOS_POLYGON) collection_type = GEOS_MULTIPOLYGON;
        else return NULL; // Should not happen

        json_t* sub_geoms_coords_array = json_object_get(geometry_json, "coordinates");
        if (!json_is_array(sub_geoms_coords_array)) {
            LOG_WARN("Coordinates for %s is not an array.", type_str); return NULL;
        }
        size_t num_sub_geoms = json_array_size(sub_geoms_coords_array);
        if (num_sub_geoms == 0) return GEOSGeom_createCollection_r(ctx, collection_type, NULL, 0);

        GEOSGeometry** geos_sub_geoms = (GEOSGeometry**)malloc(num_sub_geoms * sizeof(GEOSGeometry*));
        if (!geos_sub_geoms) { LOG_ERROR("Malloc failed for %s sub-geometries.", type_str); return NULL; }

        for (size_t i = 0; i < num_sub_geoms; ++i) {
            json_t* temp_geom_json = json_object(); // Create temp GeoJSON for sub-geometry
            json_object_set_new(temp_geom_json, "type", json_string(single_type_str));
            json_object_set(temp_geom_json, "coordinates", json_array_get(sub_geoms_coords_array, i)); // Borrow

            geos_sub_geoms[i] = json_to_geos_single_or_multi(temp_geom_json, ctx, single_type_str); // Recursive call for single type

            json_object_del(temp_geom_json, "coordinates"); // Release borrow
            json_decref(temp_geom_json);

            if (!geos_sub_geoms[i]) {
                LOG_WARN("Failed to convert sub-geometry %zu for %s.", i, type_str);
                for (size_t j = 0; j < i; ++j) GEOSGeom_destroy_r(ctx, geos_sub_geoms[j]);
                free(geos_sub_geoms); return NULL;
            }
        }
        GEOSGeometry* collection = GEOSGeom_createCollection_r(ctx, collection_type, geos_sub_geoms, num_sub_geoms);
        // GEOSGeom_createCollection_r takes ownership of elements in geos_sub_geoms. Free the array.
        free(geos_sub_geoms);
        return collection;
    }
    return NULL; // Should be unreachable
}

static GEOSGeometry* json_to_geos_geometrycollection(json_t* geometry_json, GEOSContextHandle_t ctx) {
    json_t* geometries_array_json = json_object_get(geometry_json, "geometries");
    if (!json_is_array(geometries_array_json)) {
        LOG_WARN("GeometryCollection 'geometries' field is not an array.");
        return NULL;
    }
    size_t num_geoms = json_array_size(geometries_array_json);
    if (num_geoms == 0) {
        return GEOSGeom_createCollection_r(ctx, GEOS_GEOMETRYCOLLECTION, NULL, 0);
    }

    GEOSGeometry** geos_geoms_array = (GEOSGeometry**)malloc(num_geoms * sizeof(GEOSGeometry*));
    if (!geos_geoms_array) {
        LOG_ERROR("Failed to allocate memory for GEOS geometries array (GeometryCollection).");
        return NULL;
    }

    for (size_t i = 0; i < num_geoms; ++i) {
        json_t* sub_geom_json = json_array_get(geometries_array_json, i);
        geos_geoms_array[i] = json_to_geos(sub_geom_json, ctx); // Recursive call for each geometry in the collection
        if (!geos_geoms_array[i]) {
            LOG_WARN("Failed to convert geometry %zu in GeometryCollection.", i);
            for (size_t j = 0; j < i; ++j) GEOSGeom_destroy_r(ctx, geos_geoms_array[j]);
            free(geos_geoms_array);
            return NULL;
        }
    }
    GEOSGeometry* collection = GEOSGeom_createCollection_r(ctx, GEOS_GEOMETRYCOLLECTION, geos_geoms_array, num_geoms);
    free(geos_geoms_array); // GEOS takes ownership of elements.
    return collection;
}


static GEOSGeometry* json_to_geos(json_t* geometry_json, GEOSContextHandle_t ctx) {
    if (!geometry_json || !json_is_object(geometry_json)) { LOG_WARN("Input is not a JSON object."); return NULL; }
    json_t* type_json = json_object_get(geometry_json, "type");
    if (!json_is_string(type_json)) { LOG_WARN("Missing or invalid 'type' field."); return NULL; }
    const char* type_str = json_string_value(type_json);

    if (strcmp(type_str, "Point") == 0 || strcmp(type_str, "LineString") == 0 || strcmp(type_str, "Polygon") == 0 ||
        strcmp(type_str, "MultiPoint") == 0 || strcmp(type_str, "MultiLineString") == 0 || strcmp(type_str, "MultiPolygon") == 0) {
        return json_to_geos_single_or_multi(geometry_json, ctx, type_str);
    }
    if (strcmp(type_str, "GeometryCollection") == 0) {
        return json_to_geos_geometrycollection(geometry_json, ctx);
    }

    LOG_WARN("json_to_geos: Unsupported geometry type: %s", type_str);
    return NULL;
}

static json_t* geos_coord_seq_to_json_array(const GEOSCoordSequence* seq, GEOSContextHandle_t ctx) {
    if (!seq) return NULL;
    unsigned int num_coords;
    if (GEOSCoordSeq_getSize_r(ctx, seq, &num_coords) == 0) return NULL;
    json_t* coords_array_json = json_array();
    if(!coords_array_json) return NULL;

    for (unsigned int i = 0; i < num_coords; ++i) {
        double x, y;
        if (GEOSCoordSeq_getX_r(ctx, seq, i, &x) == 0 || GEOSCoordSeq_getY_r(ctx, seq, i, &y) == 0) {
            json_decref(coords_array_json); return NULL;
        }
        json_t* pt_json = json_pack("[ff]", x, y); // Use "ff" for double, not "f,f"
        if (!pt_json) {
            json_decref(coords_array_json); return NULL;
        }
        json_array_append_new(coords_array_json, pt_json);
    }
    return coords_array_json;
}

static json_t* geos_to_json_single_or_multi(const GEOSGeometry* geos_geom, GEOSContextHandle_t ctx, GEOSGeomTypes geom_type_id) {
    const char* type_str = NULL;
    json_t* coords_json = NULL;

    if (geom_type_id == GEOS_POINT) {
        type_str = "Point";
        const GEOSCoordSequence* seq = GEOSGeom_getCoordSeq_r(ctx, geos_geom);
        if (!seq) return NULL;
        double x,y;
        if (GEOSCoordSeq_getX_r(ctx,seq,0,&x)==0 || GEOSCoordSeq_getY_r(ctx,seq,0,&y)==0) return NULL;
        coords_json = json_pack("[ff]", x, y);
    } else if (geom_type_id == GEOS_LINESTRING) {
        type_str = "LineString";
        const GEOSCoordSequence* seq = GEOSGeom_getCoordSeq_r(ctx, geos_geom);
        coords_json = geos_coord_seq_to_json_array(seq, ctx);
    } else if (geom_type_id == GEOS_POLYGON) {
        type_str = "Polygon";
        const GEOSGeometry* exterior_ring_geom = GEOSGetExteriorRing_r(ctx, geos_geom);
        if (!exterior_ring_geom) return NULL;
        const GEOSCoordSequence* exterior_seq = GEOSGeom_getCoordSeq_r(ctx, exterior_ring_geom);
        json_t* exterior_coords_json = geos_coord_seq_to_json_array(exterior_seq, ctx);
        if (!exterior_coords_json) return NULL;

        coords_json = json_array(); // This will hold all rings
        if(!coords_json) { json_decref(exterior_coords_json); return NULL; }
        json_array_append_new(coords_json, exterior_coords_json);

        int num_interior_rings = GEOSGetNumInteriorRings_r(ctx, geos_geom);
        if (num_interior_rings < 0) { json_decref(coords_json); return NULL; }
        for (int i = 0; i < num_interior_rings; ++i) {
            const GEOSGeometry* interior_ring_geom = GEOSGetInteriorRingN_r(ctx, geos_geom, i);
            if (!interior_ring_geom) { json_decref(coords_json); return NULL; }
            const GEOSCoordSequence* interior_seq = GEOSGeom_getCoordSeq_r(ctx, interior_ring_geom);
            json_t* interior_coords_json = geos_coord_seq_to_json_array(interior_seq, ctx);
            if (!interior_coords_json) { json_decref(coords_json); return NULL; }
            json_array_append_new(coords_json, interior_coords_json);
        }
    } else if (geom_type_id == GEOS_MULTIPOINT || geom_type_id == GEOS_MULTILINESTRING || geom_type_id == GEOS_MULTIPOLYGON) {
        if(geom_type_id == GEOS_MULTIPOINT) type_str = "MultiPoint";
        else if(geom_type_id == GEOS_MULTILINESTRING) type_str = "MultiLineString";
        else type_str = "MultiPolygon";

        int num_geoms = GEOSGetNumGeometries_r(ctx, geos_geom);
        if (num_geoms < 0) return NULL;
        coords_json = json_array(); // This will hold array of coordinate arrays
        if (!coords_json) return NULL;

        for (int i = 0; i < num_geoms; ++i) {
            const GEOSGeometry* sub_geom = GEOSGetGeometryN_r(ctx, geos_geom, i);
            if (!sub_geom) { json_decref(coords_json); return NULL; }
            json_t* sub_geom_full_json = geos_to_json(sub_geom, ctx); // Recursive call
            if (!sub_geom_full_json) { json_decref(coords_json); return NULL; }
            json_t* sub_coords = json_object_get(sub_geom_full_json, "coordinates");
            if (!sub_coords) { json_decref(sub_geom_full_json); json_decref(coords_json); return NULL; }
            json_incref(sub_coords); // Take ownership
            json_array_append_new(coords_json, sub_coords);
            json_decref(sub_geom_full_json);
        }
    } else {
        return NULL; // Should not be called for GeometryCollection here
    }

    if (!coords_json) return NULL;
    return json_pack("{s:s, s:o}", "type", type_str, "coordinates", coords_json);
}

static json_t* geos_to_json_geometrycollection(const GEOSGeometry* geos_geom, GEOSContextHandle_t ctx) {
    int num_geometries = GEOSGetNumGeometries_r(ctx, geos_geom);
    if (num_geometries < 0) {
        LOG_ERROR("GEOSGetNumGeometries_r failed for GeometryCollection.");
        return NULL;
    }
    json_t* geometries_array_json = json_array();
    if (!geometries_array_json) {
        LOG_ERROR("Failed to create JSON array for geometries in GeometryCollection.");
        return NULL;
    }

    for (int i = 0; i < num_geometries; ++i) {
        const GEOSGeometry* sub_geom = GEOSGetGeometryN_r(ctx, geos_geom, i); // Borrowed ref
        if (!sub_geom) {
            LOG_WARN("GEOSGetGeometryN_r failed for sub-geometry %d in GeometryCollection.", i);
            json_decref(geometries_array_json);
            return NULL;
        }
        json_t* sub_geom_json = geos_to_json(sub_geom, ctx); // Recursive call
        if (!sub_geom_json) {
            LOG_WARN("Failed to convert sub-geometry %d to JSON in GeometryCollection.", i);
            json_decref(geometries_array_json);
            return NULL;
        }
        json_array_append_new(geometries_array_json, sub_geom_json); // sub_geom_json is now owned by array
    }
    return json_pack("{s:s, s:o}", "type", "GeometryCollection", "geometries", geometries_array_json);
}


static json_t* geos_to_json(const GEOSGeometry* geos_geom, GEOSContextHandle_t ctx) {
    if (!geos_geom) return NULL;
    GEOSGeomTypes geom_type_id = GEOSGeomTypeId_r(ctx, geos_geom);

    switch (geom_type_id) {
        case GEOS_POINT:
        case GEOS_LINESTRING:
        case GEOS_POLYGON:
        case GEOS_MULTIPOINT:
        case GEOS_MULTILINESTRING:
        case GEOS_MULTIPOLYGON:
            return geos_to_json_single_or_multi(geos_geom, ctx, geom_type_id);
        case GEOS_GEOMETRYCOLLECTION:
            return geos_to_json_geometrycollection(geos_geom, ctx);
        default:
            LOG_WARN("geos_to_json: Unsupported GEOS geometry type ID: %d", geom_type_id);
            return NULL;
    }
}

GeometryValidationResultStatus validate_and_repair_geometry(
    json_t* input_geometry_json,
    const GeometricValidatorConfig* val_config,
    json_t** output_geometry_json_ptr) {

    if (!geos_context_handle) {
        LOG_ERROR("GEOS context not initialized. Call geometric_validator_init() first.");
        return GEOMETRY_STATUS_ERROR_INTERNAL;
    }
    if (!input_geometry_json || !val_config || !output_geometry_json_ptr) {
        LOG_ERROR("Null arguments to validate_and_repair_geometry.");
        return GEOMETRY_STATUS_ERROR_INTERNAL;
    }

    *output_geometry_json_ptr = input_geometry_json;

    if (!val_config->enabled) {
        return GEOMETRY_STATUS_VALIDATION_DISABLED;
    }

    GEOSGeometry* geos_geom = json_to_geos(input_geometry_json, geos_context_handle);
    if (!geos_geom) {
        LOG_ERROR("Failed to convert input GeoJSON to GEOS geometry.");
        return GEOMETRY_STATUS_ERROR_INPUT_FORMAT;
    }

    char is_valid = GEOSisValid_r(geos_context_handle, geos_geom);
    if (is_valid) {
        GEOSGeom_destroy_r(geos_context_handle, geos_geom);
        return GEOMETRY_STATUS_VALID;
    }

    char* reason = GEOSisValidReason_r(geos_context_handle, geos_geom);
    LOG_WARN("Geometry is invalid. Reason: %s", reason ? reason : "Unknown");
    if(reason) GEOSfree_r(geos_context_handle, reason);


    if (!val_config->enable_repair) {
        GEOSGeom_destroy_r(geos_context_handle, geos_geom);
        return GEOMETRY_STATUS_INVALID;
    }

    LOG_DEBUG("Attempting repair with buffer(0).");
    GEOSGeometry* repaired_geom = GEOSBuffer_r(geos_context_handle, geos_geom, 0.0, 0);
    GEOSGeom_destroy_r(geos_context_handle, geos_geom);

    if (!repaired_geom || GEOSisEmpty_r(geos_context_handle, repaired_geom)) {
        LOG_WARN("Repair attempt (buffer(0)) resulted in NULL or empty geometry.");
        if(repaired_geom) GEOSGeom_destroy_r(geos_context_handle, repaired_geom);
        return GEOMETRY_STATUS_REPAIR_FAILED;
    }

    char is_repaired_valid = GEOSisValid_r(geos_context_handle, repaired_geom);
    if (is_repaired_valid) {
        json_t* repaired_json = geos_to_json(repaired_geom, geos_context_handle);
        GEOSGeom_destroy_r(geos_context_handle, repaired_geom);
        if (repaired_json) {
            *output_geometry_json_ptr = repaired_json;
            return GEOMETRY_STATUS_REPAIRED_VALID;
        } else {
            LOG_ERROR("Failed to convert repaired GEOS geometry back to GeoJSON.");
            return GEOMETRY_STATUS_ERROR_INTERNAL;
        }
    } else {
        reason = GEOSisValidReason_r(geos_context_handle, repaired_geom);
        LOG_WARN("Repaired geometry is still invalid. Reason: %s", reason ? reason : "Unknown");
        if(reason) GEOSfree_r(geos_context_handle, reason);
        GEOSGeom_destroy_r(geos_context_handle, repaired_geom);
        return GEOMETRY_STATUS_REPAIR_FAILED;
    }
}
