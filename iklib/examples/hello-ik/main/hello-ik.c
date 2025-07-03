#include <stdio.h>
#include <ik/ik.h>

void app_main(void);

void app_main(void)
{
    printf("Hello, IK!\n");
    /* Create a solver using the FABRIK algorithm */
    struct ik_solver_t *solver = ik.solver.create(IK_FABRIK);

    /* Create a simple 3-bone structure */
    struct ik_node_t *root = solver->node->create(0);
    struct ik_node_t *child1 = solver->node->create_child(root, 1);
    struct ik_node_t *child2 = solver->node->create_child(child1, 2);
    struct ik_node_t *child3 = solver->node->create_child(child2, 3);

    /* Set node positions in local space so they form a straight line in the Y direction*/
    child1->position = ik.vec3.vec3(0, 10, 0);
    child2->position = ik.vec3.vec3(0, 10, 0);
    child3->position = ik.vec3.vec3(0, 10, 0);

    /* Attach an effector at the end */
    struct ik_effector_t *eff = solver->effector->create();
    solver->effector->attach(eff, child3);

    /* set the target position of the effector to be somewhere within range */
    eff->target_position = ik.vec3.vec3(2, -3, 5);

    /* We want to calculate rotations as well as positions */
    solver->flags |= IK_ENABLE_TARGET_ROTATIONS;

    /* Assign our tree to the solver, rebuild data and calculate solution */
    ik.solver.set_tree(solver, root);
    ik.solver.rebuild(solver);
    ik.solver.solve(solver);

    printf("target position: %f, %f, %f\n", eff->target_position.x, eff->target_position.y, eff->target_position.z);
    printf("target rotation: %f, %f, %f, %f\n\n", eff->target_rotation.x, eff->target_rotation.y, eff->target_rotation.z, eff->target_rotation.w);

    printf("child1 position: %f, %f, %f\n", child1->position.x, child1->position.y, child1->position.z);
    printf("child2 position: %f, %f, %f\n", child2->position.x, child2->position.y, child2->position.z);
    printf("child3 position: %f, %f, %f\n", child3->position.x, child3->position.y, child3->position.z);
    printf("\n");
    printf("child1 rotation: %f, %f, %f, %f\n", child1->rotation.x, child1->rotation.y, child1->rotation.z, child1->rotation.w);
    printf("child2 rotation: %f, %f, %f, %f\n", child2->rotation.x, child2->rotation.y, child2->rotation.z, child2->rotation.w);
    printf("child3 rotation: %f, %f, %f, %f\n", child3->rotation.x, child3->rotation.y, child3->rotation.z, child3->rotation.w);
}
