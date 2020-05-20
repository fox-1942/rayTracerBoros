#version 460 core

layout(std140, binding=0) buffer primitives{
    vec4 primitiveCoordinates[];
};

struct FlatBvhNode
{
// base aligment                aligned offset
    vec4 min;// 16 byte              0
    vec4 max;// 16 byte             16
    int  order;// 4 byte            32
    int  isLeaf;// 4 byte           36
    int  createdEmpty;// 4 byte     40
    int  leftOrRight;
    vec4 indices[100];// 32 byte     48
};

layout(std430, binding=2) buffer TNodes
{
    FlatBvhNode nodes[];
};

out vec4 FragColor;
in vec3 p;
uniform vec3 wEye;

struct Light{
    vec3 Le, La;
    vec3 direction;
    vec3 position;
};

uniform Light lights[];

struct Ray{
    vec3 orig, dir;
};

struct Hit{
    vec3 orig, dir, normal;
    float t;
};

Hit rayTriangleIntersect(Ray ray, vec3 v0, vec3 v1, vec3 v2){

    Hit hit;
    hit.t=-1;
    float t; float u; float v;
    vec3 v0v1 = v1 - v0;
    vec3 v0v2 = v2 - v0;
    vec3 pvec = cross(ray.dir, v0v2);
    float det = dot(v0v1, pvec);


    float invDet = 1 / det;

    vec3 tvec = ray.orig - v0;
    u = dot(tvec, pvec) * invDet;
    if (u < 0 || u > 1){
        hit.t=-1;
        return hit;
    }

    vec3 qvec = cross(tvec, v0v1);
    v = dot(ray.dir, qvec) * invDet;
    if (v < 0 || u + v > 1) {
        hit.t=-1;
        return hit;
    }

    hit.t = dot(v0v2, qvec) * invDet;
    hit.orig=ray.orig+normalize(ray.dir)*hit.t;
    hit.normal= cross(v0v1, v0v2);
    return hit;
}

vec4 getCoordinatefromIndices(float index){
    return primitiveCoordinates[int(index)];
}


FlatBvhNode leftChild(FlatBvhNode node){
    return nodes[2*node.order+1];
}

FlatBvhNode rightChild(FlatBvhNode node){
    return nodes[2*node.order+2];
}

bool rayIntersectWithBox(vec4 boxMin, vec4 boxMax, Ray r) {
    vec3 invdir = 1.0 / r.dir.xyz;
    vec3 f = (boxMax.xyz - r.orig.xyz) * invdir;
    vec3 n = (boxMin.xyz - r.orig.xyz) * invdir;

    vec3 tmax = f * sign(invdir);
    vec3 tmin = n * sign(invdir);

    return tmin.x < tmax.x && tmin.y < tmax.y && tmin.z < tmax.z;
}


Hit traverseBvhNode(Ray ray, FlatBvhNode node){
    Hit besthit;
    besthit.t=-1;
    bool hit;
    Hit hitreal;
    int i=0;

    while (i<=nodes.length()) {

        // Ha találat van és leaf a node, akkor bejárjuk, hogy van-e intersection.
        if (nodes[i].isLeaf==1){
            for (int j=0;j<nodes[i].indices.length();j++){
                if (mod(nodes[i].indices[j].x, 1)==0 && mod(nodes[i].indices[j].y, 1)==0 && mod(nodes[i].indices[j].z, 1)==0){
                    vec3 TrianglePointA=getCoordinatefromIndices(nodes[i].indices[j].x).xyz;
                    vec3 TrianglePointB=getCoordinatefromIndices(nodes[i].indices[j].y).xyz;
                    vec3 TrianglePointC=getCoordinatefromIndices(nodes[i].indices[j].z).xyz;

                    hitreal=rayTriangleIntersect(ray, TrianglePointA, TrianglePointB, TrianglePointC);

                    if (hitreal.t==-1){ continue; }

                    if (hitreal.t>0 && (besthit.t>hitreal.t || besthit.t<0)){
                        besthit=hitreal;
                    }
                }
            }
            // Ha bal oldali a leaf, akkor átmegyünk a jobb oldaliba.
            if (nodes[i].leftOrRight==0){
                i=i+1;
                continue;
            }

            // Ha jobb oldali a leaf, akkor feljebb megyünk addig, amíg a szülőnek nem lesz jobb oldali testvére
            else if (nodes[i].leftOrRight==1){

                int id=int(ceil(i-2)/2);
                FlatBvhNode parent=nodes[id];

                while (parent.leftOrRight==1){
                    parent=nodes[int(ceil(parent.order-2)/2)];
                    if (parent.order==0){
                        return besthit;
                    }
                }
                i = parent.order+1;
                continue;
            }
        }


        hit = rayIntersectWithBox(nodes[i].min, nodes[i].max, ray);

        if (hit) {
            // Ha találat van és nem leaf a node, akkor megyünk lejjebb.
            if (nodes[i].isLeaf==0){
                i=2*i+1;
                continue;
            }
        }

        else {
            // Ha nincs találat és a root node-nál vagyunk, akkor kilépés.
            if (nodes[i].order==0){
                break;
            }

            // Ha nincs találat és nem a root node-nál vagyunk és bal oldali node-nál vagyunk.
            if (nodes[i].leftOrRight==0) {
                i=i+1;
                continue;
            }

            // Ha nincs találat és nem a root node-nál vagyunk és jobb oldali node-nál vagyunk.
            else if (nodes[i].leftOrRight==1){
                FlatBvhNode parent=nodes[int(ceil(i-2)/2)];

                while (parent.leftOrRight==1){
                    parent=nodes[int(ceil(parent.order-2)/2)];
                    if (parent.order==0){
                        if (parent.order==0){
                            return besthit;
                        }
                    }
                }
                i = parent.order+1;
                continue;
            }
        }
    }
    return besthit;
}


Hit traverseBvhTree(Ray ray){
    return traverseBvhNode(ray, nodes[0]);
}

vec3 trace(Ray ray){
    const float epsilon = 0.0001f;
    vec3 outRadiance = vec3(0, 0, 0);
    vec3 gold_ka = vec3(0.24725f, 0.1995f, 0.0745f);
    vec3 gold_kd = vec3 (40.75164f, 0.60648f, 0.22648f);
    vec3 gold_ks = vec3 (0.628281f, 0.555802f, 0.366065f);
    float goldshininess= 0.4f;

    Hit hit=traverseBvhTree(ray);
    // Ha nincs metszés az objektummal oda ambiens fényt teszek.
    if (hit.t<0){ return lights[0].La;}

    // Ha van metszés az objektummal oda ambiens fény mellett ambiens fényforrás is teszek.
    // mert különben fekete lenne az egész objektum
    outRadiance+= gold_ka * lights[0].La;

    Ray shadowRay;
    shadowRay.orig = hit.orig + normalize(hit.normal) * epsilon;
    shadowRay.dir  = normalize(lights[0].direction);

    // Mivel normalizált az alábbi sor skaláris szorzótényezői, ezért az eredmény megegyezik az általuk bezárt szög cos-val
    // ha kisebb, mint nulla a cosTheta, akkor a szög nagyobb 90 foknál, ezért az objektum önmagának árnyékol.
    float cosTheta = dot(normalize(hit.normal), normalize(lights[0].direction));
    if (cosTheta>0 && traverseBvhTree(shadowRay).t<0) {
        outRadiance +=lights[0].La * gold_kd * cosTheta;
        vec3 halfway = normalize(-ray.dir + lights[0].direction);
        float cosDelta = dot(normalize(hit.normal), halfway);
        if (cosDelta > 0) outRadiance += lights[0].Le * gold_ks * pow(cosDelta, goldshininess);
    }
    return outRadiance;
}

void main()
{
    Ray ray;
    ray.orig = wEye;
    ray.dir = normalize(p - wEye);
    FragColor = vec4(trace(ray), 1);

    // FragColor = vec4(nodes[4].isLeaf, 1, 1, 1);
}